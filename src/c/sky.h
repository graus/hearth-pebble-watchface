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

// What the watch asks the phone for (bits of the SkyRequest value).
#define SKY_NEED_SUN 1
#define SKY_NEED_WEATHER 2

// Asks the phone for fresh values of what `needs` names. The watch's own
// settings decide this, not the phone's copy of them, which can be missing
// or stale (new phone, cleared app data). Fails quietly if the phone is away.
void sky_request(uint8_t needs);

// The last weather received, if any: wind -2..2, flame height 1..10.
bool sky_weather(int8_t *wind, uint8_t *flame_height);

// Flame height for the time of day, if sun times have been received: low by
// day, high at night, ramping linearly across sunrise and sunset.
bool sky_sun_height(const struct tm *now, uint8_t *flame_height);
