#pragma once
#include <pebble.h>

// Bump when the struct layout changes; stored settings with another version
// are ignored and the defaults used instead.
#define SETTINGS_VERSION 1

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

typedef enum {
  TurbulenceCalm = 0,
  TurbulenceNormal,
  TurbulenceWild,
  TurbulenceCount,
} Turbulence;

typedef struct __attribute__((__packed__)) {
  uint8_t version;
  uint8_t fps;            // 5, 8, 10, 15 or 20
  uint8_t animate;        // AnimateMode
  uint8_t resolution;     // cell size shift: 0 = 1 px, 1 = 2 px, 2 = 4 px (color only)
  uint8_t flame_height;   // 1..10
  int8_t wind;            // -2..2 (color only)
  uint8_t turbulence;     // Turbulence (color only)
  uint8_t theme;          // Theme (color only)
  uint8_t custom[3];      // ember, flame, hot as GColor8 argb (color only)
  uint8_t flare;          // bool
  uint8_t tilt_wind;      // bool (color only)
  uint8_t backlight;      // bool (emery only)
  uint8_t flicker;        // 0..100 % (emery only)
  uint8_t crackle;        // bool (emery, flint)
  uint8_t volume;         // 1..10 (emery, flint)
} Settings;

// Loads stored settings or the defaults.
void settings_load(Settings *s);
void settings_save(const Settings *s);

// Reads every setting present in an AppMessage into s. Returns true if any
// value was found.
bool settings_from_message(Settings *s, DictionaryIterator *iter);

// Top heat for the fire engine from the 1..10 flame height.
uint8_t settings_top_heat(const Settings *s);
