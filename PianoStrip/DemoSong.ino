#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "Config.h"
#include "Songs.h"

static size_t gSongIndex = 0;
static size_t gSongStep = 0;
static unsigned long gLastStepMs = 0;
static unsigned long gToneStopMs = 0;
static char gStepBuffer[64];

const char* readStepFromProgmem(size_t songIndex, size_t stepIndex) {
  PGM_P stepsPtr = (PGM_P)pgm_read_ptr(&kSongs[songIndex].steps);
  PGM_P stepPtr = (PGM_P)pgm_read_ptr(&stepsPtr[stepIndex]);
  if (!stepPtr) {
    gStepBuffer[0] = '\0';
    return gStepBuffer;
  }
  strncpy_P(gStepBuffer, stepPtr, sizeof(gStepBuffer) - 1);
  gStepBuffer[sizeof(gStepBuffer) - 1] = '\0';
  return gStepBuffer;
}

int noteLetterToSemitone(char letter) {
  switch (letter) {
    case 'C': return 0;
    case 'D': return 2;
    case 'E': return 4;
    case 'F': return 5;
    case 'G': return 7;
    case 'A': return 9;
    case 'B': return 11;
    default: return -1;
  }
}

int parseMidiFromToken(const char* token) {
  if (!token || !token[0]) {
    return -1;
  }
  int semitone = noteLetterToSemitone(token[0]);
  if (semitone < 0) {
    return -1;
  }
  int index = 1;
  if (token[index] == '#') {
    semitone += 1;
    index += 1;
  } else if (token[index] == 'b') {
    semitone -= 1;
    index += 1;
  }
  int octave = atoi(&token[index]);
  int midi = (octave + 1) * 12 + semitone;
  return midi;
}

float midiToFrequency(int midi) {
  return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

int parseFirstMidi(const char* step) {
  if (!step || !step[0]) {
    return -1;
  }
  char buffer[32];
  strncpy(buffer, step, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  char* token = strtok(buffer, \"+\");
  if (!token) {
    return -1;
  }
  while (*token == ' ') {
    token++;
  }
  return parseMidiFromToken(token);
}

void playChordStep(const char* step) {
  clearStrip();
  if (!step || !step[0]) {
    showStrip();
    return;
  }

  char buffer[32];
  strncpy(buffer, step, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  char* token = strtok(buffer, "+");
  while (token != nullptr) {
    while (*token == ' ') {
      token++;
    }
    int midi = parseMidiFromToken(token);
    if (midi >= 0) {
      lightMidiNote(midi);
    }
    token = strtok(nullptr, "+");
  }
  showStrip();
}

void advanceSongStep() {
  const Song& song = kSongs[gSongIndex];
  const char* step = readStepFromProgmem(gSongIndex, gSongStep);
  playChordStep(step);
  int midi = parseFirstMidi(step);
  if (midi >= 0) {
    float freq = midiToFrequency(midi);
    if (freq > 0.0f) {
      tone(kBuzzerPin, (unsigned int)freq, 200);
      gToneStopMs = millis() + 200;
    }
  }
  gSongStep = (gSongStep + 1) % song.stepCount;
}

void updateSongDemo(char* detailLine, size_t detailSize) {
  const Song& song = kSongs[gSongIndex];
  unsigned long now = millis();
  float stepSeconds = song.stepSeconds > 0.0f ? song.stepSeconds : kDefaultStepSeconds;
  unsigned long stepMs = (unsigned long)(stepSeconds * 1000.0f);

  if (now - gLastStepMs >= stepMs) {
    gLastStepMs = now;
    advanceSongStep();
  }
  if (gToneStopMs > 0 && now >= gToneStopMs) {
    noTone(kBuzzerPin);
    gToneStopMs = 0;
  }

  char nameBuffer[24];
  strncpy_P(nameBuffer, (PGM_P)song.name, sizeof(nameBuffer) - 1);
  nameBuffer[sizeof(nameBuffer) - 1] = '\0';
  snprintf(detailLine, detailSize, "Song: %s", nameBuffer);
}

void resetSongDemo() {
  gSongStep = 0;
  gLastStepMs = 0;
  clearStrip();
  showStrip();
}
