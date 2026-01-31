#pragma once

// ---------- Pins ----------
static const uint8_t kLcdRs = 9;
static const uint8_t kLcdEn = 8;
static const uint8_t kLcdD4 = 7;
static const uint8_t kLcdD5 = 6;
static const uint8_t kLcdD6 = 4;
static const uint8_t kLcdD7 = 3;

static const uint8_t kEncoderClk = 44;
static const uint8_t kEncoderDt = 46;

static const uint8_t kLedStripPin = 24;
static const uint16_t kLedCount = 60;

static const uint8_t kBuzzerPin = 13;

// ---------- Music / LED mapping ----------
// MIDI note that maps to LED index 0 (C2 = 36 is a good default for 60-key strip)
static const int kLedBaseMidi = 36;

// LCD refresh rate
static const unsigned long kLcdRefreshMs = 200;

// Song playback default (override per song)
static const float kDefaultStepSeconds = 0.12f;
