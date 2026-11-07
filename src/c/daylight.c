#include "daylight.h"

static int min_int(int a, int b) {
  return a < b ? a : b;
}

// How far inside the day (positive) or outside it (negative) `t` is, in
// minutes from the nearer of sunrise and sunset.
static int inside(int t, int rise, int set) {
  return min_int(t - rise, set - t);
}

int daylight(int now, int rise, int set, int ramp) {
  if (rise >= set) {
    return 0;  // the sun does not rise today
  }
  if (rise <= 0 && set >= 1440) {
    return DAYLIGHT_FULL;  // the sun does not set today
  }
  // The same day pattern repeats daily, so also look at now as a time in
  // yesterday's or tomorrow's day: just after midnight can still be the tail
  // of a sunset that falls after midnight, and just before midnight the start
  // of a sunrise that falls before it.
  int best = inside(now, rise, set);
  int t = inside(now + 1440, rise, set);
  if (t > best) best = t;
  t = inside(now - 1440, rise, set);
  if (t > best) best = t;
  if (ramp < 1) ramp = 1;
  int light = (best + ramp / 2) * DAYLIGHT_FULL / ramp;
  return light < 0 ? 0 : (light > DAYLIGHT_FULL ? DAYLIGHT_FULL : light);
}
