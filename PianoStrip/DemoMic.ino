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

void setupMatrix() {
  gMatrix.shutdown(0, false);
  gMatrix.setIntensity(0, 8);
  gMatrix.clearDisplay(0);
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

int freqToMidi(float freq) {
  if (freq <= 0) {
    return -1;
  }
  return (int)lroundf(69.0f + 12.0f * log(freq / 440.0f) / log(2.0f));
}

void findTopBins(int* bins, int binCount) {
  for (int i = 0; i < binCount; i++) {
    bins[i] = -1;
  }
  for (int i = 2; i < (int)(kFftSamples / 2); i++) {
    float mag = gVReal[i];
    int insertAt = -1;
    for (int j = 0; j < binCount; j++) {
      if (bins[j] < 0 || mag > gVReal[bins[j]]) {
        insertAt = j;
        break;
      }
    }
    if (insertAt < 0) {
      continue;
    }
    for (int j = binCount - 1; j > insertAt; j--) {
      bins[j] = bins[j - 1];
    }
    bins[insertAt] = i;
  }
}

void updateMicDemo(char* detailLine, size_t detailSize) {
  sampleFftBlock();
  gFft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  gFft.compute(FFT_FORWARD);
  gFft.complexToMagnitude();

  int topBins[4];
  findTopBins(topBins, 4);

  clearStrip();
  int mainMidi = -1;
  char notesLine[32] = "Mic: ";
  size_t notesLen = strlen(notesLine);

  for (int i = 0; i < 4; i++) {
    int bin = topBins[i];
    if (bin < 0) {
      continue;
    }
    float freq = (float)bin * kFftSampleRate / (float)kFftSamples;
    int midi = freqToMidi(freq);
    if (midi < 0) {
      continue;
    }
    if (mainMidi < 0) {
      mainMidi = midi;
    }
    int pc = midi % 12;
    if (pc < 0) {
      pc += 12;
    }
    lightMidiNote(midi);
    const char* name = kNoteNames[pc];
    size_t nameLen = strlen(name);
    if (notesLen + nameLen + 2 < sizeof(notesLine)) {
      memcpy(notesLine + notesLen, name, nameLen);
      notesLen += nameLen;
      notesLine[notesLen++] = ' ';
      notesLine[notesLen] = '\0';
    }
  }

  showStrip();

  if (mainMidi >= 0) {
    int mainPc = mainMidi % 12;
    if (mainPc < 0) {
      mainPc += 12;
    }
    drawChar(kNoteNames[mainPc][0]);
  } else {
    drawChar(' ');
  }

  if (notesLen == strlen("Mic: ")) {
    snprintf(detailLine, detailSize, "Mic: --");
  } else {
    snprintf(detailLine, detailSize, "%s", notesLine);
  }
}
