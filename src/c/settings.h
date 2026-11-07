#pragma once
#include <pebble.h>

// Bump when the struct layout changes; stored settings with another version
// are ignored and the defaults used instead.
#define SETTINGS_VERSION 3

typedef enum {
  AnimateAlways = 0,
  AnimateOnTap = 1,
} AnimateMode;

typedef enum {
  ThemeClassic = 0,
  ThemeEmber,
  ThemeBlue,
  ThemeGreen,
  ThemePurple,
  ThemeCustom,
  ThemeCount,
} Theme;

// What sets the flame height.
typedef enum {
  HeightManual = 0,   // the Flame height setting
  HeightWeather = 1,  // outside temperature: colder is higher
  HeightSun = 2,      // low by day, high at night, ramping at sunrise and sunset
  HeightSourceCount,
} HeightSource;

typedef struct __attribute__((__packed__)) {
  uint8_t version;
  uint8_t fps;            // 5, 8, 10, 15 or 20
  uint8_t animate;        // AnimateMode
  uint8_t resolution;     // cell size shift: 0 = 1 px, 1 = 2 px, 2 = 4 px (color only)
  uint8_t flame_height;   // 1..10
  int8_t wind;            // -2..2 (color only)
  uint8_t reserved;       // was turbulence (v1, v2); Doom's fire has none
  uint8_t theme;          // Theme (color only)
  uint8_t custom[3];      // ember, flame, hot as GColor8 argb (color only)
  uint8_t flare;          // bool
  uint8_t tilt_wind;      // bool (color only)
  uint8_t backlight;      // bool (emery only)
  uint8_t flicker;        // 0..100 % (emery only)
  uint8_t crackle;        // bool (emery, flint)
  uint8_t volume;         // 1..10 (emery, flint)
  uint8_t stoke;          // bool: a tap or shake makes the flames leap
  uint8_t height_source;  // HeightSource (v2: bool "weather", same values)
  uint8_t weather_wind;   // bool: the local wind sets the wind (color only)
} Settings;

// Loads stored settings or the defaults.
void settings_load(Settings *s);
void settings_save(const Settings *s);

// Reads every setting present in an AppMessage into s. Returns true if any
// value was found.
bool settings_from_message(Settings *s, DictionaryIterator *iter);

// Reads one integer from an AppMessage, whatever width and signedness the
// phone used (or a decimal string). Returns false if the key is missing.
bool settings_read_int(DictionaryIterator *iter, uint32_t key, int32_t *out);

// Top heat for the fire engine from a 1..10 flame height.
uint8_t settings_top_heat(uint8_t flame_height);
