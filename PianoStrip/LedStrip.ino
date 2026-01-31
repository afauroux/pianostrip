#include <FastLED.h>
#include "Config.h"

CRGB gLeds[kLedCount];

void setupLedStrip() {
  FastLED.addLeds<WS2812B, kLedStripPin, GRB>(gLeds, kLedCount);
  FastLED.setBrightness(100);
  FastLED.clear(true);
}

void clearStrip() {
  fill_solid(gLeds, kLedCount, CRGB::Black);
}

int midiToLedIndex(int midiNote) {
  int index = midiNote - kLedBaseMidi;
  if (index < 0 || index >= (int)kLedCount) {
    return -1;
  }
  return index;
}

int pitchClassFromMidi(int midiNote) {
  int pc = midiNote % 12;
  if (pc < 0) {
    pc += 12;
  }
  return pc;
}

void lightMidiNote(int midiNote) {
  int ledIndex = midiToLedIndex(midiNote);
  if (ledIndex < 0) {
    return;
  }
  int pc = pitchClassFromMidi(midiNote);
  uint8_t hue = (uint8_t)(pc * 21);
  gLeds[ledIndex] = CHSV(hue, 255, 255);
}

void showStrip() {
  FastLED.show();
}

void showRainbowDemo(uint8_t offset) {
  for (uint16_t i = 0; i < kLedCount; i++) {
    gLeds[i] = CHSV((offset + i) * 4, 255, 255);
  }
}
