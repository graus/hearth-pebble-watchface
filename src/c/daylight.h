#pragma once
// How much daylight there is at a time of day, from that day's sunrise and
// sunset. Plain C with no Pebble dependency so it can be tested on the host.

#include <stdint.h>

#define DAYLIGHT_FULL 256

// now: minutes after local midnight, 0..1439.
// rise, set: minutes after local midnight of today. Sunrise can fall before
// midnight (negative) and sunset after it (above 1440), as happens far north
// in summer. rise == set means the sun does not rise today (polar night);
// rise <= 0 && set >= 1440 means it does not set (midnight sun).
// ramp: minutes the change takes, centered on sunrise and on sunset.
// Returns 0 (night) .. DAYLIGHT_FULL (day).
int daylight(int now, int rise, int set, int ramp);
