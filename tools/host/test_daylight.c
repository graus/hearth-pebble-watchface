// Host tests for the sunrise/sunset ramp (src/c/daylight.c).

#include <stdio.h>

#include "../../src/c/daylight.h"

static int s_failures;

#define CHECK(cond, ...) do { \
  if (!(cond)) { s_failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } \
} while (0)

#define FULL DAYLIGHT_FULL

int main(void) {
  // An ordinary day: sunrise 07:00, sunset 19:00, a 60-minute ramp.
  int rise = 7 * 60, set = 19 * 60;
  CHECK(daylight(3 * 60, rise, set, 60) == 0, "night before dawn");
  CHECK(daylight(12 * 60, rise, set, 60) == FULL, "noon");
  CHECK(daylight(23 * 60, rise, set, 60) == 0, "night after dusk");
  CHECK(daylight(rise, rise, set, 60) == FULL / 2, "sunrise is halfway");
  CHECK(daylight(set, rise, set, 60) == FULL / 2, "sunset is halfway");
  CHECK(daylight(rise - 30, rise, set, 60) == 0, "ramp starts 30 min before sunrise");
  CHECK(daylight(rise + 30, rise, set, 60) == FULL, "ramp ends 30 min after sunrise");
  int prev = -1;
  for (int t = rise - 40; t <= rise + 40; t++) {
    int d = daylight(t, rise, set, 60);
    CHECK(d >= prev, "dawn never darkens (%d at %d)", d, t);
    prev = d;
  }

  // Sunset after midnight (Reykjavik in June: 02:55 to 00:03 next day).
  rise = 2 * 60 + 55;
  set = 1440 + 3;
  CHECK(daylight(23 * 60 + 50, rise, set, 60) > 0, "still dusk at 23:50");
  CHECK(daylight(1440 - 1, rise, set, 60) > FULL / 3, "dusk centered on 00:03, not 24:00");
  CHECK(daylight(2, rise, set, 60) > 0 && daylight(2, rise, set, 60) < FULL,
        "just after midnight is the tail of the dusk ramp");
  CHECK(daylight(60, rise, set, 60) == 0, "01:00 is night");
  CHECK(daylight(12 * 60, rise, set, 60) == FULL, "noon");

  // Sunrise before midnight (the mirror case).
  rise = -10;
  set = 21 * 60;
  CHECK(daylight(1430, rise, set, 60) > 0, "dawn starts before midnight");
  CHECK(daylight(0, rise, set, 60) > FULL / 2, "just past sunrise at midnight");

  // Polar night and midnight sun.
  CHECK(daylight(12 * 60, 0, 0, 60) == 0, "polar night");
  CHECK(daylight(0, 0, 1440, 60) == FULL && daylight(12 * 60, 0, 1440, 60) == FULL, "midnight sun");

  // Every minute of a few days stays in range.
  int days[][2] = {{420, 1140}, {175, 1443}, {-10, 1260}, {0, 0}, {0, 1440}, {600, 900}};
  for (unsigned i = 0; i < sizeof(days) / sizeof(days[0]); i++) {
    for (int t = 0; t < 1440; t++) {
      int d = daylight(t, days[i][0], days[i][1], 60);
      CHECK(d >= 0 && d <= FULL, "range %d at %d", d, t);
    }
  }
  printf("daylight: %d failures\n", s_failures);
  return s_failures ? 1 : 0;
}
