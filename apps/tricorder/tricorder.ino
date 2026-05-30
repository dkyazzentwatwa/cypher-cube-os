// Tricorder — one-screen sensor dashboard: battery (AXP2101), clock (PCF85063),
// IMU (QMI8658) accel/gyro/orientation, and strongest nearby WiFi RSSI.
// Tap to trigger a fresh WiFi scan. Long-press BOOT returns to launcher.

#include <WiFi.h>

#include <WaveshareAmoledAppKit.h>
#include <WaveshareAmoledSensors.h>

namespace WA = WaveshareAmoled;
namespace WS = WaveshareAmoled::Sensors;

namespace {
int bestRssi = 0;
char bestSsid[33] = "";
uint32_t lastScanAt = 0;

const char* orientName(WS::Orientation o) {
  switch (o) {
    case WS::OrientFaceUp: return "face up";
    case WS::OrientFaceDown: return "face down";
    case WS::OrientPortrait: return "portrait";
    case WS::OrientPortraitInverted: return "portrait^";
    case WS::OrientLandscapeLeft: return "landsc L";
    case WS::OrientLandscapeRight: return "landsc R";
    default: return "unknown";
  }
}

void scanWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  const int n = WiFi.scanNetworks(false, true, false, 120);
  bestRssi = 0;
  bestSsid[0] = '\0';
  for (int i = 0; i < n; i++) {
    if (bestSsid[0] == '\0' || WiFi.RSSI(i) > bestRssi) {
      bestRssi = WiFi.RSSI(i);
      strncpy(bestSsid, WiFi.SSID(i).c_str(), sizeof(bestSsid) - 1);
      bestSsid[sizeof(bestSsid) - 1] = '\0';
    }
  }
  WiFi.scanDelete();
  lastScanAt = millis();
}

void drawDash() {
  Adafruit_GFX& d = WA::display();
  WA::clear();
  WA::header("Tricorder");
  d.setTextSize(2);
  int y = 58;
  auto line = [&](const char* s, uint16_t c) {
    d.setTextColor(c);
    d.setCursor(16, y);
    d.print(s);
    y += 30;
  };
  char buf[40];

  // Battery.
  if (WS::Battery::available()) {
    const int pct = WS::Battery::percent();
    snprintf(buf, sizeof(buf), "Batt: %d%% %u mV %s", pct,
             WS::Battery::voltageMv(), WS::Battery::isCharging() ? "CHG" : "");
  } else {
    snprintf(buf, sizeof(buf), "Batt: n/a");
  }
  line(buf, WA::kColorText);

  // Clock.
  if (WS::Rtc::available()) {
    WS::DateTime t = WS::Rtc::now();
    snprintf(buf, sizeof(buf), "Time: %04u-%02u-%02u %02u:%02u", t.year, t.month,
             t.day, t.hour, t.minute);
  } else {
    snprintf(buf, sizeof(buf), "Time: no RTC");
  }
  line(buf, WA::kColorText);

  // IMU.
  WS::Vec3 a, g;
  if (WS::Imu::readAccel(a) && WS::Imu::readGyro(g)) {
    snprintf(buf, sizeof(buf), "Acc: %+.2f %+.2f %+.2f", a.x, a.y, a.z);
    line(buf, WA::kColorDim);
    snprintf(buf, sizeof(buf), "Gyr: %+.0f %+.0f %+.0f", g.x, g.y, g.z);
    line(buf, WA::kColorDim);
    snprintf(buf, sizeof(buf), "Orient: %s", orientName(WS::Imu::orientation()));
    line(buf, WA::kColorDim);
  } else {
    line("IMU: n/a", WA::kColorDim);
  }

  // WiFi.
  y += 6;
  if (bestSsid[0]) {
    snprintf(buf, sizeof(buf), "WiFi: %d dBm", bestRssi);
    line(buf, WA::kColorAccent);
    snprintf(buf, sizeof(buf), "  %s", bestSsid);
    line(buf, WA::kColorAccent);
  } else {
    line("WiFi: tap to scan", WA::kColorAccent);
  }

  WA::footer("Tap: WiFi scan", "BOOT hold: home");
}
}  // namespace

void setup() {
  WA::begin("Tricorder");
  WS::begin();
  drawDash();
}

void loop() {
  WA::Event e = WA::poll();
  if (WA::isHome(e)) WA::returnToLauncher();
  if (e.type == WA::EventTap) {
    WA::message("Scanning", "Scanning WiFi...", WA::kColorAccent);
    scanWifi();
  }
  drawDash();
  delay(500);
}
