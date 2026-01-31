#include <FastLED.h>

#define LED_PIN     24
#define NUM_LEDS    60
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(100); // 0–255
}
int count=0;
void loop() {
  // Turn all LEDs off
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }

  // Example: light each LED with a different color
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV((count+i) * 4, 255, 255); // rainbow
  }
  count++;

  FastLED.show();
  delay(50);
}
