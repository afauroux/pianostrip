
// include the library code:
#include <LiquidCrystal.h>
#include <arduinoFFT.h>

// ================= LCD =================
const int rs = 9, en = 8, d4 = 7, d5 = 6, d6 = 4, d7 = 3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
static unsigned long lastLcd = 0;


// ================= LEDS =================
const int ledPins[] = {A1, A2, A3, A4, A0, A7, A9, A6, A8, A13, 32, 36, 40, 38};
//const int ledNotes[] ={ 7,  6,  5,  4,  3,  2,  1,  0, 11,  10,  9,  8,  7,  6};
const int ledNotes[] ={A6, A9, A7, A0, A4, A3, A2, A1, 36,  32, A13, A8};
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);
float ledLevel[12] = {0};          // 0..255 (smoothed)
const float ATTACK = 0.5f;         // how fast it rises (0..1)
const float DECAY  = 0.1f;         // how fast it falls (0..1) smaller = longer memory


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

  // Optional: focus on chord-ish region (recommended)
  // kMin = (int)ceil(60.0 * N / Fs);
  // kMax = (int)floor(1200.0 * N / Fs);

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
    //digitalWrite(ledPins[i], LOW); // Sink current
    digitalWrite(ledPins[i], HIGH); // immediately force OFF (for active-LOW)
  }

  lcd.begin(16, 2);
  lcd.clear();

  setupBinToPc();
  //Serial.begin(115200);
}

static unsigned long lastReport = 0;

void loopDebug() {
  unsigned long t0, t1, t2, t3, t4;

  t0 = micros();
  sampleBlock();
  t1 = micros();

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  t2 = micros();

  FFT.compute(FFTDirection::Forward);
  t3 = micros();

  FFT.complexToMagnitude();
  computeChroma();
  t4 = micros();

  // DO NOT do lcd.clear()/lcd.print/Serial prints before this point

  if (millis() - lastReport >= 1000) {
    lastReport = millis();
    Serial.print("sample: "); Serial.print(t1 - t0);
    Serial.print("  window: "); Serial.print(t2 - t1);
    Serial.print("  fft: "); Serial.print(t3 - t2);
    Serial.print("  mag+chroma: "); Serial.println(t4 - t3);
  }

  // Now do LCD / LEDs here (not in the timed section)
}


const unsigned long FRAME_MS = 80;   // try 80–200
static unsigned long lastFrame = 0;

void loop() {
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame += FRAME_MS;

  // Debug time
  unsigned long t = micros();

  // 1) Sample
  sampleBlock();
  //Serial.println(micros()-t);

  // 2) FFT
  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  //Serial.println(micros()-t);
  computeChroma();

  //chromaSmooth[i]
  //chromaToUint8();


  // for (int pc = 0; pc < 12; pc++) {
  //   float target = chroma8[pc];                 // 0..255 current strength
  //   if(target<150){target=0;}
  //   float a = (target > ledLevel[pc]) ? ATTACK : DECAY;  // fast up, slow down
  //   ledLevel[pc] += a * (target - ledLevel[pc]);         // EMA with decay
  //   Serial.print(ledLevel[pc]);
  //   Serial.print(", ");
  // }
  // Serial.println("");

  int pcs[5];
  topPitchClasses(pcs);

  // float maxMag = 0;
  // float avgMag = 0;

  // int kMin = 2;
  // int kMax = (N/2) - 2;

  // for (int k = kMin; k <= kMax; k++) {
  //   float m = vReal[k];
  //   avgMag += m;
  //   if (m > maxMag) maxMag = m;
  // }
  // avgMag /= (kMax - kMin + 1);

  //Serial.println(String(avgMag)+" "+String(maxMag));
  
  //Serial.print("Free RAM: ");
  //Serial.println(freeRam());

  // 3) Pick peaks
  //findTopPeaks4(maxMag, avgMag);

  // Fixed frame threshold: ignore weak sound entirely
  //if (maxMag < FRAME_MIN_MAXMAG) {
  //  for (int i = 0; i < 4; i++) peakBins[i] = -1;
    //return; // skip peak picking + notes
  //}

  for (int i = 0; i < 12; i++) {
    digitalWrite(ledNotes[i], HIGH);
  } 
  lcd.setCursor(0, 0);
  for(int j=0; j<5;j++){
    int i = pcs[j];
    if(i>=0){
      lcd.print(noteNames[i]);
      uint8_t level = (uint16_t)chroma8[i] * 5 / 255;  // 0..5
      lcd.write(level);  // prints custom char 0..5
      
      digitalWrite(ledNotes[i], LOW); 
    }else{
      lcd.print("-- ");
    }
  }



  
  // int f1 = binToHz(peakBins[0]);
  // int f2 = binToHz(peakBins[1]);
  // int f3 = binToHz(peakBins[2]);
  // int f4 = binToHz(peakBins[3]);

  // int n1 = freqToNote(f1);
  // int n2 = freqToNote(f2);
  // int n3 = freqToNote(f3);
  // int n4 = freqToNote(f4);

  // for (int i = 0; i < numLeds; i++) {
  //   pinMode(ledPins[i], OUTPUT);
  //   digitalWrite(ledPins[i], HIGH); // Sink current
  // }

  // // 4) Display (compact)
  // //lcd.clear(); // speed up
  // lcd.setCursor(0, 0);

  // if (f1 > 0) { digitalWrite(ledNotes[n1], LOW);}
  // if (f2 > 0) { digitalWrite(ledNotes[n2], LOW);}
  // if (f3 > 0) { digitalWrite(ledNotes[n3], LOW);}
  // if (f4 > 0) { digitalWrite(ledNotes[n4], LOW);}

  // // line 1: f1 f2
  // if (f1 > 0) { lcd.print(noteNames[n1]); lcd.print(" "); } else lcd.print("-- ");
  // if (f2 > 0) { lcd.print(noteNames[n2]); } else lcd.print("--");

  // lcd.setCursor(0, 1);
  // // line 2: f3 f4
  // if (f3 > 0) { lcd.print(noteNames[n3]); lcd.print(" "); } else lcd.print("-- ");
  // if (f4 > 0) { lcd.print(noteNames[n4]); } else lcd.print("--");

  // Update rate (LCD is slow). 5–10 Hz is plenty.
  //delay(100);


}

