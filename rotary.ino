#include <RotaryEncoder.h>

RotaryEncoder enc(44, 46);
const int ledPins[] = {A1, A2, A3, A4, A0, A7, A9, A6, A8, A13, 32, 36, 40, 38};
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);



void setup() {
  Serial.begin(115200);
  pinMode(44, INPUT_PULLUP);
  pinMode(46, INPUT_PULLUP);
  for(int i=0;i<numLeds;i++){
    pinMode(ledPins[i], INPUT_PULLUP);
    digitalWrite(ledPins[i], HIGH);
  }
  for (int i = 0; i < numLeds; i++) {
  pinMode(ledPins[i], OUTPUT);
}

}

int lastCLK = HIGH;
int count = 0;
void loop() {
  for(int i=0;i<numLeds;i++){
    digitalWrite(ledPins[i], HIGH);
  }
  int clk = digitalRead(44);
  if (clk != lastCLK) {
    if (digitalRead(46) != clk){
      Serial.println("+"); 
      count+=1;
    }else{
      Serial.println("-");
      count+=-1;
    } 
  }
  Serial.println(abs(count % numLeds));
  digitalWrite(ledPins[int(abs(count % numLeds))], LOW);
  lastCLK = clk;
  delay(0);
}



void loopOld2() {
  Serial.print(digitalRead(44));
  Serial.print(" ");
  Serial.println(digitalRead(46));
  delay(50);
}
int lastPos = 0;
void loopOld() {
  enc.tick();
  int pos = enc.getPosition();
  Serial.println(pos);
  if (pos != lastPos) {
    lastPos = pos;
    Serial.println("yo");
  }
  delay(100);
}
