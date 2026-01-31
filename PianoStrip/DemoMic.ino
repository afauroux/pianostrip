#include <arduinoFFT.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "LedControl.h"
#include "font8x8_basic.h"
#include "Config.h"

static float gVReal[kFftSamples];
static float gVImag[kFftSamples];
static ArduinoFFT<float> gFft(gVReal, gVImag, kFftSamples, kFftSampleRate);

static LedControl gMatrix(kMatrixDin, kMatrixClk, kMatrixCs, 1);

static const char* kNoteNames[12] = {
  "C", "C#", "D", "D#", "E", "F",
  "F#", "G", "G#", "A", "A#", "B"
};

static float gChroma[12];
static float gChromaSmooth[12];
static int8_t gBinToPc[kFftSamples / 2];
static float gBinOctaveWeight[kFftSamples / 2];

static const float kChromaAlpha = 0.30f;
static const float kChromaThreshold = 4.0f;

void setupMatrix() {
  gMatrix.shutdown(0, false);
  gMatrix.setIntensity(0, 8);
  gMatrix.clearDisplay(0);
}

void setupBinToPc() {
  for (int k = 0; k < (int)(kFftSamples / 2); k++) {
    float f = (float)k * kFftSampleRate / (float)kFftSamples;
    if (f < 60.0f || f > 2500.0f) {
      gBinToPc[k] = -1;
      gBinOctaveWeight[k] = 0.0f;
      continue;
    }
    int midi = (int)lroundf(69.0f + 12.0f * log(f / 440.0f) / log(2.0f));
    int pc = (midi % 12 + 12) % 12;
    int octave = midi / 12;
    gBinToPc[k] = (int8_t)pc;
    gBinOctaveWeight[k] = 1.0f / (1.0f + 0.15f * octave);
  }
}

void setupMic() {
  setupMatrix();
  setupBinToPc();
}

void drawChar(char c) {
  if (c < 0 || c > 127) {
    return;
  }
  for (int i = 0; i < 8; i++) {
    gMatrix.setColumn(0, i, font8x8_basic[(int)c][i]);
  }
}

void sampleFftBlock() {
  const unsigned long intervalUs = (unsigned long)(1000000.0f / kFftSampleRate);
  unsigned long next = micros();
  long sum = 0;
  for (uint16_t i = 0; i < kFftSamples; i++) {
    while ((long)(micros() - next) < 0) {}
    next += intervalUs;
    int sample = analogRead(kMicAnalogPin);
    gVReal[i] = (float)sample;
    gVImag[i] = 0.0f;
    sum += sample;
  }
  float mean = (float)sum / (float)kFftSamples;
  for (uint16_t i = 0; i < kFftSamples; i++) {
    gVReal[i] -= mean;
  }
}

void computeChroma() {
  for (int i = 0; i < 12; i++) {
    gChroma[i] = 0.0f;
  }

  const float kFmin = 100.0f;
  const float kFmax = 2500.0f;

  int kMin = (int)ceilf(kFmin * (float)kFftSamples / kFftSampleRate);
  int kMax = (int)floorf(kFmax * (float)kFftSamples / kFftSampleRate);
  if (kMin < 2) kMin = 2;
  int kNy = (kFftSamples / 2) - 2;
  if (kMax > kNy) kMax = kNy;

  for (int k = kMin; k <= kMax; k++) {
    float mag = gVReal[k];
    if (mag <= 0.0f) {
      continue;
    }
    int pc = gBinToPc[k];
    if (pc < 0) {
      continue;
    }
    float f = (float)k * kFftSampleRate / (float)kFftSamples;
    float w = 1.0f / (1.0f + 0.001f * f);
    float contrib = mag * w * gBinOctaveWeight[k];
    if (contrib > gChroma[pc]) {
      gChroma[pc] = contrib;
    }
  }

  for (int i = 0; i < 12; i++) {
    gChromaSmooth[i] =
      (1.0f - kChromaAlpha) * gChromaSmooth[i] +
      kChromaAlpha * gChroma[i];
  }
}

void topPitchClasses(int outPc[4]) {
  bool used[12] = {false};
  for (int n = 0; n < 4; n++) {
    float best = -1.0f;
    int bestPc = -1;
    for (int pc = 0; pc < 12; pc++) {
      if (!used[pc] && gChromaSmooth[pc] > best) {
        best = gChromaSmooth[pc];
        bestPc = pc;
      }
    }
    if (best > kChromaThreshold) {
      outPc[n] = bestPc;
      used[bestPc] = true;
    } else {
      outPc[n] = -1;
    }
  }
}

int midiFromPitchClass(int pc) {
  return kLedBaseMidi + pc;
}

void updateMicDemo(char* detailLine, size_t detailSize) {
  sampleFftBlock();
  gFft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  gFft.compute(FFT_FORWARD);
  gFft.complexToMagnitude();

  computeChroma();

  int pcs[4];
  topPitchClasses(pcs);

  clearStrip();
  int mainPc = -1;
  char notesLine[32] = "";
  size_t notesLen = 0;

  for (int i = 0; i < 4; i++) {
    int pc = pcs[i];
    if (pc < 0) {
      continue;
    }
    if (mainPc < 0) {
      mainPc = pc;
    }
    int midi = midiFromPitchClass(pc);
    lightMidiNote(midi);
  }

  showStrip();

  for (int i = 0; i < 4; i++) {
    int pc = pcs[i];
    if (pc >= 0) {
      const char* name = kNoteNames[pc];
      size_t nameLen = strlen(name);
      if (notesLen + nameLen + 2 < sizeof(notesLine)) {
        memcpy(notesLine + notesLen, name, nameLen);
        notesLen += nameLen;
        notesLine[notesLen++] = ' ';
        notesLine[notesLen] = '\0';
      }
    } else {
      if (notesLen + 3 < sizeof(notesLine)) {
        memcpy(notesLine + notesLen, "-- ", 3);
        notesLen += 3;
        notesLine[notesLen] = '\0';
      }
    }
  }

  if (mainPc >= 0) {
    drawChar(kNoteNames[mainPc][0]);
  } else {
    drawChar(' ');
  }

  if (notesLen == 0) {
    snprintf(detailLine, detailSize, "-- -- -- --");
  } else {
    snprintf(detailLine, detailSize, "%s", notesLine);
  }
}
