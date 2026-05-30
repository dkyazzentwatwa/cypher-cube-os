#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <string.h>

namespace WaveshareAmoledIntro {

namespace detail {

constexpr uint8_t kRainCols = 28;
constexpr uint16_t kBg = 0x0000;
constexpr uint16_t kPlate = 0x0021;
constexpr uint16_t kPlateEdge = 0x05FF;
constexpr uint16_t kMatrixDim = 0x0320;
constexpr uint16_t kMatrixMid = 0x07E0;
constexpr uint16_t kMatrixHead = 0x87FF;
constexpr uint16_t kScan = 0xFFE0;
constexpr uint16_t kText = 0xFFFF;

inline uint16_t textPixelWidth(const char* text, uint8_t textSize) {
  return strlen(text) * 6 * textSize;
}

inline int16_t centeredTextX(uint16_t width, const char* text, uint8_t textSize) {
  const uint16_t tw = textPixelWidth(text, textSize);
  return max(0, (static_cast<int>(width) - static_cast<int>(tw)) / 2);
}

inline void resetRain(int16_t rainY[kRainCols], uint8_t rainSpeed[kRainCols],
                      uint16_t height) {
  randomSeed(micros());
  for (uint8_t i = 0; i < kRainCols; i++) {
    rainY[i] = random(-static_cast<int>(height), 0);
    rainSpeed[i] = random(7, 15);
  }
}

template <typename DisplayT>
inline void drawRain(DisplayT& display, uint16_t width, uint16_t height,
                     int16_t rainY[kRainCols], uint8_t rainSpeed[kRainCols]) {
  static constexpr char glyphs[] = "01#*+<>/";
  const int16_t colW = max(8, static_cast<int>(width) / kRainCols);
  display.setTextSize(1);
  for (uint8_t col = 0; col < kRainCols; col++) {
    const int16_t x = col * colW + (colW / 2);
    for (uint8_t tail = 0; tail < 8; tail++) {
      const int16_t y = rainY[col] - tail * 14;
      if (y < -8 || y >= static_cast<int>(height)) continue;
      const uint16_t color = tail == 0 ? kMatrixHead :
                             tail < 3 ? kMatrixMid : kMatrixDim;
      display.setTextColor(color);
      display.setCursor(x, y);
      display.write(glyphs[(col + tail + (millis() / 120)) %
                           (sizeof(glyphs) - 1)]);
    }
    rainY[col] += rainSpeed[col];
    if (rainY[col] - 110 > static_cast<int>(height)) {
      rainY[col] = random(-80, 0);
      rainSpeed[col] = random(7, 15);
    }
  }
}

}  // namespace detail

template <typename DisplayT>
inline void draw(DisplayT& display, uint16_t width, uint16_t height,
                 const char* title, const char* credit = "by littlehakr",
                 uint32_t durationMs = 4000) {
  if (!title || !title[0] || width == 0 || height == 0) return;
  if (!credit) credit = "";

  int16_t rainY[detail::kRainCols];
  uint8_t rainSpeed[detail::kRainCols];
  detail::resetRain(rainY, rainSpeed, height);

  static constexpr uint32_t kFrameMs = 70;
  const uint32_t revealCandidate = durationMs * 45 / 100;
  const uint32_t revealMs = revealCandidate < 800 ? 800 : revealCandidate;
  const uint32_t typeStartCandidate = durationMs / 2;
  const uint32_t typeStartMs = typeStartCandidate < revealMs ? revealMs : typeStartCandidate;
  const uint32_t typeCandidate = durationMs * 28 / 100;
  const uint32_t typeMs = typeCandidate < 600 ? 600 : typeCandidate;

  const int16_t plateX = 18;
  const int16_t plateY = height < 230 ? 40 : (static_cast<int16_t>(height) - 150) / 2;
  const int16_t plateW = width - plateX * 2;
  const int16_t plateH = 150;
  const int16_t creditY = plateY + 104;

  uint8_t titleSize = 4;
  uint16_t titleW = 0;
  uint16_t titleH = 0;
  do {
    titleW = detail::textPixelWidth(title, titleSize);
    titleH = 8 * titleSize;
    if (titleW <= width - 48 || titleSize <= 2) break;
    titleSize--;
  } while (true);
  const int16_t titleY = plateY + (titleSize >= 4 ? 44 : 52);
  const int16_t titleX = detail::centeredTextX(width, title, titleSize);
  const int16_t creditX = detail::centeredTextX(width, credit, 2);
  const uint32_t startedAt = millis();

  display.fillScreen(detail::kBg);
  detail::drawRain(display, width, height, rainY, rainSpeed);

  while (millis() - startedAt < durationMs) {
    const uint32_t frameStart = millis();
    const uint32_t elapsed = frameStart - startedAt;

    display.fillRoundRect(plateX, plateY, plateW, plateH, 8, detail::kPlate);
    display.drawRoundRect(plateX, plateY, plateW, plateH, 8, detail::kPlateEdge);
    display.drawFastHLine(plateX + 14, plateY + 22, plateW - 28, detail::kMatrixDim);
    display.drawFastHLine(plateX + 14, plateY + plateH - 22, plateW - 28,
                          detail::kMatrixDim);

    const uint32_t revealElapsed = min(elapsed, revealMs);
    const int16_t revealW =
        (static_cast<uint32_t>(titleW + 12) * revealElapsed) / revealMs;
    const int16_t beamX = titleX + revealW;

    display.setTextSize(titleSize);
    display.setTextColor(detail::kText);
    display.setCursor(titleX, titleY);
    display.print(title);
    if (revealW < static_cast<int>(titleW + 12)) {
      display.fillRect(beamX, titleY - 4, width - beamX - plateX,
                       static_cast<int>(titleH) + 12, detail::kPlate);
      display.drawFastVLine(beamX, titleY - 8, titleH + 18, detail::kScan);
      if (beamX + 2 < static_cast<int>(width) - plateX) {
        display.drawFastVLine(beamX + 2, titleY - 4, titleH + 10,
                              detail::kMatrixHead);
      }
    }

    if (elapsed >= typeStartMs && credit[0]) {
      char shown[40];
      const uint32_t typeElapsed = min(elapsed - typeStartMs, typeMs);
      uint8_t shownLen = (strlen(credit) * typeElapsed) / typeMs;
      if (shownLen > sizeof(shown) - 1) shownLen = sizeof(shown) - 1;
      memcpy(shown, credit, shownLen);
      shown[shownLen] = '\0';
      display.setTextSize(2);
      display.setTextColor(detail::kMatrixHead);
      display.setCursor(creditX, creditY);
      display.print(shown);
      if ((millis() / 250) & 1) {
        display.drawFastVLine(creditX + shownLen * 12 + 4, creditY, 16,
                              detail::kScan);
      }
    }

    const uint32_t spent = millis() - frameStart;
    if (spent < kFrameMs) delay(kFrameMs - spent);
  }

  display.fillRoundRect(plateX, plateY, plateW, plateH, 8, detail::kPlate);
  display.drawRoundRect(plateX, plateY, plateW, plateH, 8, detail::kPlateEdge);
  display.drawFastHLine(plateX + 14, plateY + 22, plateW - 28, detail::kMatrixDim);
  display.drawFastHLine(plateX + 14, plateY + plateH - 22, plateW - 28,
                        detail::kMatrixDim);
  display.setTextSize(titleSize);
  display.setTextColor(detail::kText);
  display.setCursor(titleX, titleY);
  display.print(title);
  if (credit[0]) {
    display.setTextSize(2);
    display.setTextColor(detail::kMatrixHead);
    display.setCursor(creditX, creditY);
    display.print(credit);
  }
  delay(250);
}

}  // namespace WaveshareAmoledIntro
