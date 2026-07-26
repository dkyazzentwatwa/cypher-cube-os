#include <WaveshareAmoledAppKit.h>
#include <WaveshareAmoledAudio.h>

using namespace WaveshareAmoled;

namespace {

constexpr const char* kDefaultClip = "/waveshare-os/audio/test.wav";
char statusLine[72] = "Put PCM WAV at /waveshare-os/audio/test.wav";
uint32_t lastDraw = 0;

void draw() {
  clear();
  header("Audio Diagnostics", WaveshareAmoledAudio::isPlaying() ? "PLAYING" : "IDLE");
  wrapped("SD playback: PCM 16-bit WAV, mono or stereo. Supported rates: 8, 16, 22.05, 32, 44.1, 48 kHz.",
          18, 72, 30, 4, kColorDim);
  button(18, 196, 332, 62, "Play test.wav", false, kColorAccent);
  button(18, 274, 158, 52, "Stop", false, kColorWarn);
  button(192, 274, 158, 52, "Vol +10", false, kColorGood);
  wrapped(statusLine, 18, 354, 30, 2, kColorText);
  footer("Tap controls", "Hold BOOT home");
  lastDraw = millis();
}

void showPlaybackResult(bool accepted) {
  if (accepted) {
    snprintf(statusLine, sizeof(statusLine), "Queued: %s", kDefaultClip);
  } else {
    snprintf(statusLine, sizeof(statusLine), "Could not play test.wav. Check SD path/format.");
  }
  draw();
}

}  // namespace

void setup() {
  begin("Audio Diagnostics", true);
  WaveshareAmoledAudio::begin();
  serialHelp("help, play, stop, volume 0-100, home");
  draw();
}

void loop() {
  WaveshareAmoledAudio::update();
  const Event event = poll();
  if (isHome(event)) returnToLauncher();
  if (event.type == EventTap) {
    if (hit(event, 18, 196, 332, 62)) {
      showPlaybackResult(WaveshareAmoledAudio::requestPlayWav(kDefaultClip));
    } else if (hit(event, 18, 274, 158, 52)) {
      WaveshareAmoledAudio::stop();
      snprintf(statusLine, sizeof(statusLine), "Stop requested");
      draw();
    } else if (hit(event, 192, 274, 158, 52)) {
      uint8_t next = WaveshareAmoledAudio::volume() + 10;
      if (next > 100 || next < WaveshareAmoledAudio::volume()) next = 100;
      WaveshareAmoledAudio::setVolume(next);
      snprintf(statusLine, sizeof(statusLine), "Volume: %u%%", next);
      draw();
    }
  } else if (event.type == EventSerialLine) {
    if (strcmp(event.line, "home") == 0) {
      returnToLauncher();
    } else if (strcmp(event.line, "play") == 0) {
      showPlaybackResult(WaveshareAmoledAudio::requestPlayWav(kDefaultClip));
    } else if (strcmp(event.line, "stop") == 0) {
      WaveshareAmoledAudio::stop();
      snprintf(statusLine, sizeof(statusLine), "Stop requested");
      draw();
    } else if (strncmp(event.line, "volume ", 7) == 0) {
      const int value = atoi(event.line + 7);
      if (value >= 0 && value <= 100) {
        WaveshareAmoledAudio::setVolume(static_cast<uint8_t>(value));
        snprintf(statusLine, sizeof(statusLine), "Volume: %d%%", value);
      } else {
        snprintf(statusLine, sizeof(statusLine), "Volume must be 0 through 100");
      }
      draw();
    } else if (strcmp(event.line, "help") == 0) {
      serialHelp("help, play, stop, volume 0-100, home");
    }
  }
  if (millis() - lastDraw > 500 && (WaveshareAmoledAudio::isPlaying() ||
                                    strcmp(statusLine, "Stop requested") == 0)) {
    draw();
  }
  delay(16);
}
