#include "backlight.h"

#if defined(PBL_RGB_BACKLIGHT)

#include <stdlib.h>

#define SAMPLES 40
#define MAX_JITTER 70
// The brightness tracks how the flames swing around their own average, in
// tenths of a heat step: +-SPAN maps to the full 0..255 range.
#define SPAN 9

static bool s_on;
// Running average of the sampled heat, tenths of a step times 16. The host
// simulation put it near 227 (22.7) with the defaults; tracking it keeps the
// flicker centered when reach or resolution change the mean.
static int32_t s_center = 227 * 16;

void backlight_update(const Fire *fire, uint8_t flicker) {
  uint16_t w = fire_width(fire), h = fire_height(fire);
  uint32_t n = (uint32_t)w * h;
  uint32_t step = n / SAMPLES;
  if (step == 0) step = 1;
  uint32_t sum = 0, count = 0;
  for (uint32_t i = step / 2; i < n; i += step) {
    sum += fire_row(fire, i / w)[i % w];
    count++;
  }
  int32_t avg10 = count ? (int32_t)(sum * 10 / count) : 0;
  s_center += avg10 - s_center / 16;

  int32_t heat = (avg10 - s_center / 16 + SPAN) * 255 / (2 * SPAN);
  int jitter = MAX_JITTER * flicker / 100;
  if (jitter) heat += rand() % (2 * jitter + 1) - jitter;
  if (heat < 0) heat = 0;
  if (heat > 255) heat = 255;

  uint32_t r = 150 + heat * 105 / 255;
  uint32_t g = 12 + heat * 140 / 255;
  uint32_t b = heat * 24 / 255;
  if (!s_on) {
    light_enable(true);
    s_on = true;
  }
  light_set_color_rgb888(r << 16 | g << 8 | b);
}

void backlight_release(void) {
  if (s_on) {
    light_set_system_color();
    light_enable(false);
    s_on = false;
  }
}

#endif
