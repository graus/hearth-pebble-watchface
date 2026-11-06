#pragma once
#include <pebble.h>

// Weather-driven fire. The phone fetches the local weather and sends it
// already mapped to the face's own units (see src/pkjs/weather.js), so the
// watch only stores and uses two small numbers.

// Loads the last weather received, which survives restarts.
void weather_load(void);

// Reads weather keys from an AppMessage and stores them. Returns true if the
// message carried weather.
bool weather_from_message(DictionaryIterator *iter);

// Asks the phone for fresh weather. Fails quietly if the phone is away.
void weather_request(void);

// The last weather received, if any: wind -2..2, flame height 1..10.
bool weather_get(int8_t *wind, uint8_t *flame_height);
