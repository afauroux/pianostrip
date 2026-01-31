#include <LiquidCrystal.h>
#include "Config.h"

LiquidCrystal gLcd(kLcdRs, kLcdEn, kLcdD4, kLcdD5, kLcdD6, kLcdD7);

static unsigned long gLastLcdRefresh = 0;
static int gEncoderLastClk = HIGH;
void setupLcd() {
  gLcd.begin(16, 2);
  gLcd.clear();
  gLcd.setCursor(0, 0);
  gLcd.print("PianoStrip");
}

void setupEncoder() {
  pinMode(kEncoderClk, INPUT_PULLUP);
  pinMode(kEncoderDt, INPUT_PULLUP);
  gEncoderLastClk = digitalRead(kEncoderClk);
}

int readEncoderDelta() {
  int delta = 0;
  int clk = digitalRead(kEncoderClk);
  if (clk != gEncoderLastClk) {
    int dt = digitalRead(kEncoderDt);
    if (dt != clk) {
      delta = 1;
    } else {
      delta = -1;
    }
    gEncoderLastClk = clk;
  }
  return delta;
}

void updateLcdMenu(const char* modeLabel, const char* detailLine) {
  unsigned long now = millis();
  if (now - gLastLcdRefresh < kLcdRefreshMs) {
    return;
  }
  gLastLcdRefresh = now;
  gLcd.setCursor(0, 0);
  gLcd.print("Mode: ");
  gLcd.print(modeLabel);
  gLcd.print("   ");
  gLcd.setCursor(0, 1);
  gLcd.print(detailLine);
  gLcd.print("                ");
}
