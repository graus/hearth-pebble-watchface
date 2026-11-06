#include "settings.h"

#include <stdlib.h>

#define SETTINGS_KEY 1

static const Settings s_defaults = {
  .version = SETTINGS_VERSION,
  .fps = 10,
  .animate = AnimateAlways,
  .resolution = 1,
  .flame_height = 5,
  .wind = 0,
  .turbulence = TurbulenceNormal,
  .theme = ThemeClassic,
  .custom = {0xF0 /* red */, 0xF4 /* orange */, 0xFC /* yellow */},
  .flare = 0,
  .tilt_wind = 0,
  .backlight = 1,
  .flicker = 100,
  .crackle = 1,
  .volume = 3,
};

static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Values are clamped as they are read, before a narrow field can wrap them
// (fps 300 must not become 44).
static uint8_t clamp_fps(int fps) {
  static const uint8_t allowed[] = {5, 8, 10, 15, 20};
  for (unsigned i = 0; i < ARRAY_LENGTH(allowed); i++) {
    if (fps <= allowed[i]) {
      return allowed[i];
    }
  }
  return 20;
}

// Stored data may come from a damaged write or a buggy older build, so every
// field is clamped to its range before use.
static void sanitize(Settings *s) {
  s->version = SETTINGS_VERSION;
  s->fps = clamp_fps(s->fps);
  s->animate = s->animate ? AnimateOnTap : AnimateAlways;
  s->resolution = clamp(s->resolution, 0, 2);
  s->flame_height = clamp(s->flame_height, 1, 10);
  s->wind = clamp(s->wind, -2, 2);
  s->turbulence = clamp(s->turbulence, 0, TurbulenceCount - 1);
  s->theme = clamp(s->theme, 0, ThemeCount - 1);
  for (int i = 0; i < 3; i++) {
    s->custom[i] |= 0xC0;  // opaque
  }
  s->flare = !!s->flare;
  s->tilt_wind = !!s->tilt_wind;
  s->backlight = !!s->backlight;
  s->flicker = clamp(s->flicker, 0, 100);
  s->crackle = !!s->crackle;
  s->volume = clamp(s->volume, 1, 10);
}

void settings_load(Settings *s) {
  *s = s_defaults;
  if (persist_exists(SETTINGS_KEY)) {
    Settings stored;
    int read = persist_read_data(SETTINGS_KEY, &stored, sizeof(stored));
    if (read == (int)sizeof(stored) && stored.version == SETTINGS_VERSION) {
      *s = stored;
    }
  }
  sanitize(s);
}

void settings_save(const Settings *s) {
  persist_write_data(SETTINGS_KEY, s, sizeof(*s));
}

// The phone sends int32 values, but accept any integer width or a decimal
// string so a hand-written config page cannot crash the face.
static bool read_int(DictionaryIterator *iter, uint32_t key, int32_t *out) {
  Tuple *t = dict_find(iter, key);
  if (!t) {
    return false;
  }
  switch (t->type) {
    case TUPLE_INT:
      *out = t->length == 1 ? t->value->int8 : (t->length == 2 ? t->value->int16 : t->value->int32);
      return true;
    case TUPLE_UINT:
      *out = t->length == 1 ? t->value->uint8 : (t->length == 2 ? t->value->uint16 : (int32_t)t->value->uint32);
      return true;
    case TUPLE_CSTRING:
      *out = atoi(t->value->cstring);
      return true;
    default:
      return false;
  }
}

static uint8_t rgb_to_argb8(int32_t rgb) {
  return GColorFromHEX((uint32_t)rgb).argb;
}

bool settings_from_message(Settings *s, DictionaryIterator *iter) {
  bool any = false;
  int32_t v;
  if (read_int(iter, MESSAGE_KEY_FrameRate, &v)) { s->fps = clamp_fps(v); any = true; }
  if (read_int(iter, MESSAGE_KEY_Animate, &v)) { s->animate = v != 0; any = true; }
  if (read_int(iter, MESSAGE_KEY_Resolution, &v)) { s->resolution = clamp(v, 0, 2); any = true; }
  if (read_int(iter, MESSAGE_KEY_FlameHeight, &v)) { s->flame_height = clamp(v, 1, 10); any = true; }
  if (read_int(iter, MESSAGE_KEY_Wind, &v)) { s->wind = clamp(v, -2, 2); any = true; }
  if (read_int(iter, MESSAGE_KEY_Turbulence, &v)) { s->turbulence = clamp(v, 0, TurbulenceCount - 1); any = true; }
  if (read_int(iter, MESSAGE_KEY_Theme, &v)) { s->theme = clamp(v, 0, ThemeCount - 1); any = true; }
  if (read_int(iter, MESSAGE_KEY_CustomEmber, &v)) { s->custom[0] = rgb_to_argb8(v); any = true; }
  if (read_int(iter, MESSAGE_KEY_CustomFlame, &v)) { s->custom[1] = rgb_to_argb8(v); any = true; }
  if (read_int(iter, MESSAGE_KEY_CustomHot, &v)) { s->custom[2] = rgb_to_argb8(v); any = true; }
  if (read_int(iter, MESSAGE_KEY_Flare, &v)) { s->flare = v != 0; any = true; }
  if (read_int(iter, MESSAGE_KEY_TiltWind, &v)) { s->tilt_wind = v != 0; any = true; }
  if (read_int(iter, MESSAGE_KEY_Backlight, &v)) { s->backlight = v != 0; any = true; }
  if (read_int(iter, MESSAGE_KEY_Flicker, &v)) { s->flicker = clamp(v, 0, 100); any = true; }
  if (read_int(iter, MESSAGE_KEY_Crackle, &v)) { s->crackle = v != 0; any = true; }
  if (read_int(iter, MESSAGE_KEY_Volume, &v)) { s->volume = clamp(v, 1, 10); any = true; }
  return any;
}

uint8_t settings_top_heat(const Settings *s) {
  // 1 -> 0, 5 -> 9, 10 -> 20, rounded.
  return ((s->flame_height - 1) * 20 + 4) / 9;
}
