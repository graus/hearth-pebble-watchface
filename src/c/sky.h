#pragma once
#include <pebble.h>

// What the sky outside says about the fire: the local weather and the times
// of sunrise and sunset. The phone works these out (src/pkjs/sky.js) and
// sends them already in the face's own units, so the watch only stores a few
// small numbers. The last values received survive restarts.

void sky_load(void);

// Reads sky keys from an AppMessage and stores them. Returns true if the
// message carried any.
bool sky_from_message(DictionaryIterator *iter);

// Asks the phone for fresh values. Fails quietly if the phone is away.
void sky_request(void);

// The last weather received, if any: wind -2..2, flame height 1..10.
bool sky_weather(int8_t *wind, uint8_t *flame_height);

// Flame height for the time of day, if sun times have been received: low by
// day, high at night, ramping linearly across sunrise and sunset.
bool sky_sun_height(const struct tm *now, uint8_t *flame_height);
