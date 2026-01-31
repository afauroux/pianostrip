// Define the buzzer pin
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978

// the id of the led to turn on
#define IDLED_C4 7
#define IDLED_D4 5
#define IDLED_E4 3
#define IDLED_F4 2
#define IDLED_G4 0
#define IDLED_A4 10
#define IDLED_AS4 9
#define IDLED_C5 7

int buzzerPin = 13;

// Define the "Happy Birthday" melody
int melody[] = {
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_F4, NOTE_E4, // "Happy Birthday to You"
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_C4, NOTE_G4, NOTE_F4, // "Happy Birthday to You"
  NOTE_C4, NOTE_C4, NOTE_C5, NOTE_A4, NOTE_F4, NOTE_E4, NOTE_D4, // "Happy Birthday dear [Name]"
  NOTE_AS4, NOTE_AS4, NOTE_A4, NOTE_F4, NOTE_G4, NOTE_F4, NOTE_C4 // "Happy Birthday to You"
};
int melodyChar[] = {
  'C', 'C', 'D', 'C', 'F', 'E', // "Happy Birthday to You"
  'C', 'C', 'D', 'C', 'G', 'F', // "Happy Birthday to You"
  'C', 'C', 'C', 'A', 'F', 'E', 'D', // "Happy Birthday dear [Name]"
  'A', 'A', 'A', 'F', 'G', 'F', 'C' // "Happy Birthday to You"
};

int ledID[] = {
  IDLED_C4, IDLED_C4, IDLED_D4, IDLED_C4, IDLED_F4, IDLED_E4, // "Happy Birthday to You"
  IDLED_C4, IDLED_C4, IDLED_D4, IDLED_C4, IDLED_G4, IDLED_F4, // "Happy Birthday to You"
  IDLED_C4, IDLED_C4, IDLED_C5, IDLED_A4, IDLED_F4, IDLED_E4, IDLED_D4, // "Happy Birthday dear [Name]"
  IDLED_AS4, IDLED_AS4, IDLED_A4, IDLED_F4, IDLED_G4, IDLED_F4, IDLED_C4 // "Happy Birthday to You"
};


// Define the note durations
int noteDurations[] = {
  4, 4, 4, 4, 4, 2,
  4, 4, 4, 4, 4, 2,
  4, 4, 4, 4, 4, 4, 2,
  4, 4, 4, 4, 4, 2, 4
};

// Note definitions for the melody
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_C5 523

// include the library code:
#include <LiquidCrystal.h>
#include <arduinoFFT.h>
#include "LedControl.h"
#include "font8x8_basic.h"

// ---- FONTS ----
void drawChar(char c) {
  for (int i = 0; i < 8; i++) {
    lc.setColumn(0, i, font8x8_basic[c][i]);
  }
}

// ================= LCD =================
const int rs = 9, en = 8, d4 = 7, d5 = 6, d6 = 4, d7 = 3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
static unsigned long lastLcd = 0;


// ---- LED MATRIX ----
// DIN, CLK, CS, devices
LedControl lc(12, 10, 11, 1);

// ---- PIANO LEDS ----
const int ledPins[] = {A1, A2, A3, A4, A0, A7, A9, A11, A12, A13, 32, 36, 40, 38};
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);


// ================= FFT =================
const uint8_t ADC_PIN = A10;        // your analog pin
const uint16_t N = 256;//512;             // 256 is safe; 512 is better if you have RAM
const float Fs = 8000.0;            // Hz (max freq you can see is Fs/2)
//const float Fs = 11025.0;            // Hz (max freq you can see is Fs/2)

const float FRAME_MIN_MAXMAG = 6.0;   // ignore whole frame if max FFT mag < this
const float PEAK_MIN_MAG     = 3.0;    // ignore individual peaks below this

// Adaptive thresholds
const float REL_TO_MAX       = 0.25;    // peak must be >= 25% of strongest peak
const float MULT_TO_AVG      = 3.0;     // peak must be >= 3x average noise floor

// buffers
float vReal[N];
float vImag[N];
ArduinoFFT<float> FFT(vReal, vImag, N, Fs);

// ================= NOTES =================
const char* noteNames[12] = {
  "C ", "C#", "D ", "D#", "E ", "F ",
  "F#", "G ", "G#", "A ", "A#", "B "
};

// ================= CHROMA =================
float chroma[12];
float chromaSmooth[12];
float bestchromaSmooth[12];
uint8_t chroma8[12];
const float TRESHOLD_CHROMA = 4.0f;
const float CHROMA_ALPHA = 0.30f;   // temporal smoothing (0.2–0.4)
int8_t binToPC[N/2];   // -1 = ignore, 0..11 = C..B
float   binOctaveWeight[N/2];

// Store peak bins
int peakBins[4];

// ================= HELPERS =================
int freqToMidi(float f) {
  if (f <= 0) return -1;
  return (int)lroundf(69.0f + 12.0f * log(f / 440.0f) / log(2.0f));
}

const int freqToNote(float freq) {
  if (freq <= 0) return "--";

  int noteNumber = (int)round(12.0 * log(freq / 440.0) / log(2.0) + 69);
  int noteIndex = (noteNumber % 12 + 12) % 12;  // safe modulo

  return noteIndex;
}

void sampleBlock() {
  const unsigned long Ts_us = (unsigned long)(1000000.0 / Fs);

  unsigned long next = micros();
  long sum = 0;

  for (uint16_t i = 0; i < N; i++) {
    // Wait until next scheduled time (handles micros overflow safely)
    while ((long)(micros() - next) < 0) {}
    next += Ts_us;

    int x = analogRead(ADC_PIN);
    vReal[i] = (float)x;
    vImag[i] = 0.0f;
    sum += x;
  }

  float mean = (float)sum / (float)N;
  for (uint16_t i = 0; i < N; i++) vReal[i] -= mean;
}



// Find top K peaks with neighbor suppression
void findTopPeaks4(float maxMag, float avgMag) {
  const int guard = 2;  // suppress +/-2 bins around a chosen peak

  int kMin = 2;
  int kMax = (N/2) - 2;

  for (int i = 0; i < 4; i++) peakBins[i] = -1;

  // If overall frame is too weak, ignore it
  if (maxMag < FRAME_MIN_MAXMAG) return;

  float adaptive = max(REL_TO_MAX * maxMag, MULT_TO_AVG * avgMag);
  float threshold = max(PEAK_MIN_MAG, adaptive);

  for (int p = 0; p < 4; p++) {
    float bestMag = threshold;
    int bestK = -1;

    for (int k = kMin; k <= kMax; k++) {
      // Skip bins near already-chosen peaks
      bool tooClose = false;
      for (int j = 0; j < p; j++) {
        if (peakBins[j] >= 0 && abs(k - peakBins[j]) <= guard) {
          tooClose = true;
          break;
        }
      }
      if (tooClose) continue;

      // Local maximum check
      float a = vReal[k - 1];
      float b = vReal[k];
      float c = vReal[k + 1];
      if (!(b > a && b >= c)) continue;

      if (b > bestMag) {
        bestMag = b;
        bestK = k;
      }
    }

    peakBins[p] = bestK;
  }

  // Sort by frequency (stable LCD)
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 4; j++) {
      if (peakBins[j] >= 0 && (peakBins[i] < 0 || peakBins[j] < peakBins[i])) {
        int tmp = peakBins[i]; peakBins[i] = peakBins[j]; peakBins[j] = tmp;
      }
    }
  }
}

int binToHz(int k) {
  if (k < 0) return -1;
  float f = (float)k * Fs / (float)N;
  return (int)lroundf(f);
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// ================= CHROMA FROM FFT =================
// Assumes you have these globals precomputed ONCE in setup():
//   int8_t binToPC[N/2];            // -1 or 0..11
//   float  binOctaveWeight[N/2];    // 0..1 (0 for ignored bins)
// And you still have:
//   float chroma[12], chromaSmooth[12];
//   const float CHROMA_ALPHA;

void computeChroma() {
  // reset chroma
  for (int i = 0; i < 12; i++) chroma[i] = 0.0f;

  // musical range (tune if needed)
  const float FMIN = 100.0f;
  const float FMAX = 2500.0f;

  int kMin = (int)ceilf(FMIN * (float)N / Fs);
  int kMax = (int)floorf(FMAX * (float)N / Fs);
  if (kMin < 2) kMin = 2;
  int kNy = (N / 2) - 2;
  if (kMax > kNy) kMax = kNy;

  for (int k = kMin; k <= kMax; k++) {
    float mag = vReal[k];
    if (mag <= 0.0f) continue;

    int pc = binToPC[k];
    if (pc < 0) continue;

    // down-weight high harmonics (no log; just use frequency from bin index)
    float f = (float)k * Fs / (float)N;
    float w = 1.0f / (1.0f + 0.001f * f);

    // octave normalization via precomputed weight
    float contrib = mag * w * binOctaveWeight[k];

    // KEEP STRONGEST CONTRIBUTION PER PITCH CLASS
    if (contrib > chroma[pc]) chroma[pc] = contrib;
  }

  // temporal smoothing
  for (int i = 0; i < 12; i++) {
    chromaSmooth[i] =
      (1.0f - CHROMA_ALPHA) * chromaSmooth[i] +
       CHROMA_ALPHA        * chroma[i];
  }
}


void chromaToUint8() {
  float maxVal = 0.0f;

  // find max chroma value
  for (int i = 0; i < 12; i++) {
    if (chromaSmooth[i] > maxVal)
      maxVal = chromaSmooth[i];
  }

  // avoid divide-by-zero
  if (maxVal < 1e-6f) {
    for (int i = 0; i < 12; i++) chroma8[i] = 0;
    return;
  }

  // normalize to 0–255
  for (int i = 0; i < 12; i++) {
    float v = chromaSmooth[i] / maxVal;   // 0..1
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    chroma8[i] = (uint8_t)(v * 255.0f);
  }
}

// ================= PICK TOP 5 =================
void topPitchClasses(int outPc[5]) {
  bool used[12] = {false};

  for (int n = 0; n < 5; n++) {
    float best = -1;
    int bestPc = -1;
    outPc[n] = -1;

    for (int pc = 0; pc < 12; pc++) {
      if (!used[pc] && chromaSmooth[pc] > best) {
        best = chromaSmooth[pc];
        bestPc = pc;
      }
    }

    if(best > TRESHOLD_CHROMA){
      outPc[n] = bestPc;
    }else{
      outPc[n] = -1;
    }
    if (bestPc >= 0) used[bestPc] = true;
  }
}

// ==================================== SETUP ==================================== 
void setupBinToPc(){
for (int k = 0; k < (int)(N/2); k++) {
  float f = (float)k * Fs / (float)N;

  // ignore non-musical range
  if (f < 60.0f || f > 2500.0f) {
    binToPC[k] = -1;
    binOctaveWeight[k] = 0.0f;
    continue;
  }

  int midi = (int)lroundf(69.0f + 12.0f * log(f / 440.0f) / log(2.0f));
  int pc   = (midi % 12 + 12) % 12;
  int octave = midi / 12;

  binToPC[k] = (int8_t)pc;

  // octave suppression (tune 0.1–0.25)
  binOctaveWeight[k] = 1.0f / (1.0f + 0.15f * octave);
}


}
void setup() {

  byte bar0[8] = {0,0,0,0,0,0,0,0};
  byte bar1[8] = {0,0,0,0,0,0,0,31};
  byte bar2[8] = {0,0,0,0,0,0,31,31};
  byte bar3[8] = {0,0,0,0,0,31,31,31};
  byte bar4[8] = {0,0,0,0,31,31,31,31};
  byte bar5[8] = {0,0,0,31,31,31,31,31};

  lcd.createChar(0, bar0);
  lcd.createChar(1, bar1);
  lcd.createChar(2, bar2);
  lcd.createChar(3, bar3);
  lcd.createChar(4, bar4);
  lcd.createChar(5, bar5);

  // set-up the leds
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }

  lcd.begin(16, 2);
  lcd.clear();

  // led matrix
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);

  setupBinToPc();
  //Serial.begin(115200);
}


int count=0;
void loop() {
  // Iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < 25; thisNote++) {
    for(int i=0;i<numLeds;i++){
      digitalWrite(ledPins[i], HIGH);
    }
    //digitalWrite(ledPins[count++%numLeds], LOW);
    

    drawChar(melodyChar[thisNote]);
    //Serial.println(ledPins[ledID[thisNote]]);
    digitalWrite(ledPins[ledID[thisNote]], LOW);
    // To calculate the note duration, take one second divided by the note type.
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(buzzerPin, melody[thisNote], noteDuration);

    // To distinguish the notes, set a minimum time between them.
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    // Stop the tone playing:
    noTone(buzzerPin);
  }
  delay(1000);
}
