#include "stdint.h"

#ifdef FASTLED_TYPE

#include <FastLED.h>

// Define the array of leds
// CRGBArray<NUM_LEDS> leds;
CRGB leds[FASTLED_NUM_LEDS];
int num_leds = -1;
#endif

void fastled_setup(void) {
#ifdef FASTLED_TYPE
  num_leds = FASTLED_NUM_LEDS;
#ifdef FASTLED_CLOCK_PIN
  FastLED.addLeds<FASTLED_TYPE, FASTLED_DATA_PIN, FASTLED_CLOCK_PIN, RGB>(
      leds, FASTLED_NUM_LEDS); // GRB ordering is typical
#else
  FastLED.addLeds<FASTLED_TYPE, FASTLED_DATA_PIN, FASTLED_COLOR_ORDER>(
      leds, FASTLED_NUM_LEDS);
#endif
#endif
}

void fastled_set(uint8_t num, const struct CRGB &color) {
#ifdef FASTLED_TYPE
  if (num < num_leds) {
    // can use individual leds
    leds[num] = color;
  }
#endif
}

void fastled_update(void) {
#ifdef FASTLED_TYPE
  FastLED.show();
#endif
}

void fastled_show(uint8_t num, const struct CRGB &color) {
#ifdef FASTLED_TYPE
  fastled_set(num, color);
  fastled_update();
#endif
}

void fastled_get(const uint8_t num, struct CRGB &color) {
#ifdef FASTLED_TYPE
  color = leds[num];
#endif
}
