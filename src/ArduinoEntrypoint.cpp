#include "Launcher.h"

// Keep firmware entrypoints in normal C++ so Arduino's sketch preprocessor
// cannot rewrite them into a recursive setup() call.
void setup() {
  launcherSetup();
}

void loop() {
  launcherLoop();
}
