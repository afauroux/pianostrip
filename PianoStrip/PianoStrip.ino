#include <Arduino.h>
#include "Config.h"

// Forward declarations from other .ino files
void setupLedStrip();
void setupLcd();
void setupEncoder();
int readEncoderDelta();
void updateLcdMenu(const char* modeLabel, const char* detailLine);
void updateRainbowDemo(char* detailLine, size_t detailSize);
void updateSongDemo(char* detailLine, size_t detailSize);
void resetSongDemo();

enum DemoMode {
  kModeRainbow = 0,
  kModeSong = 1,
  kModeCount
};

static const char* kModeLabels[] = {
  "Rainbow",
  "Song"
};

static DemoMode gMode = kModeRainbow;

void setup() {
  setupLedStrip();
  setupLcd();
  setupEncoder();
}

void updateMode(int delta) {
  if (delta == 0) {
    return;
  }
  int mode = (int)gMode + delta;
  if (mode < 0) {
    mode += kModeCount;
  }
  if (mode >= kModeCount) {
    mode -= kModeCount;
  }
  gMode = static_cast<DemoMode>(mode);
  if (gMode == kModeSong) {
    resetSongDemo();
  }
}

void loop() {
  int delta = readEncoderDelta();
  updateMode(delta);

  char detailLine[32] = "";
  switch (gMode) {
    case kModeRainbow:
      updateRainbowDemo(detailLine, sizeof(detailLine));
      break;
    case kModeSong:
      updateSongDemo(detailLine, sizeof(detailLine));
      break;
    default:
      break;
  }

  updateLcdMenu(kModeLabels[gMode], detailLine);
}
