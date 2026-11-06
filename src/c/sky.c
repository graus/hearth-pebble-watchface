#include "sky.h"

#define SKY_KEY 3
#define SKY_VERSION 1

// Flame heights for the sun source, and how long the change takes. The ramp
// is centered on sunrise and sunset, so dusk and dawn sit halfway.
#define DAY_HEIGHT 3
#define NIGHT_HEIGHT 8
#define RAMP_MIN 60

#define HAS_WEATHER 1
#define HAS_SUN 2

typedef struct __attribute__((__packed__)) {
  uint8_t version;
  uint8_t has;            // HAS_WEATHER | HAS_SUN
  int8_t wind;            // -2..2
  uint8_t flame_height;   // 1..10
  uint16_t sunrise;       // minutes after local midnight
  uint16_t sunset;        // equal to sunrise: no sunrise today (polar night)
} Sky;

static Sky s_sky;

void sky_load(void) {
  Sky stored;
  if (persist_read_data(SKY_KEY, &stored, sizeof(stored)) == (int)sizeof(stored) &&
      stored.version == SKY_VERSION) {
    s_sky = stored;
  }
}

static bool read_int(DictionaryIterator *iter, uint32_t key, int32_t *out) {
  Tuple *t = dict_find(iter, key);
  if (!t || (t->type != TUPLE_INT && t->type != TUPLE_UINT)) {
    return false;
  }
  *out = t->length == 1 ? t->value->int8 : (t->length == 2 ? t->value->int16 : t->value->int32);
  return true;
}

static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

bool sky_from_message(DictionaryIterator *iter) {
  bool any = false;
  int32_t a, b;
  // Pairs arrive together; ignore half a pair rather than mix old and new.
  if (read_int(iter, MESSAGE_KEY_SkyWind, &a) && read_int(iter, MESSAGE_KEY_SkyHeight, &b)) {
    s_sky.wind = clamp(a, -2, 2);
    s_sky.flame_height = clamp(b, 1, 10);
    s_sky.has |= HAS_WEATHER;
    any = true;
  }
  if (read_int(iter, MESSAGE_KEY_SunRise, &a) && read_int(iter, MESSAGE_KEY_SunSet, &b)) {
    s_sky.sunrise = clamp(a, 0, 1440);
    s_sky.sunset = clamp(b, 0, 1440);
    s_sky.has |= HAS_SUN;
    any = true;
  }
  if (any) {
    s_sky.version = SKY_VERSION;
    persist_write_data(SKY_KEY, &s_sky, sizeof(s_sky));
  }
  return any;
}

void sky_request(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;  // busy or no phone: the next half-hour tick tries again
  }
  dict_write_int8(iter, MESSAGE_KEY_SkyRequest, 1);
  app_message_outbox_send();
}

bool sky_weather(int8_t *wind, uint8_t *flame_height) {
  if (!(s_sky.has & HAS_WEATHER)) {
    return false;
  }
  *wind = s_sky.wind;
  *flame_height = s_sky.flame_height;
  return true;
}

// 0 at night .. 256 in full day. Minutes from the nearest sunrise or sunset
// decide how far along the ramp we are.
static int daylight(int now, int rise, int set) {
  if (rise == set) {
    return 0;  // the sun does not rise today
  }
  if (rise == 0 && set == 1440) {
    return 256;  // the sun does not set today
  }
  // Signed distance into the day from each edge, positive inside the day.
  int from_rise = now - rise;
  int to_set = set - now;
  int inside = from_rise < to_set ? from_rise : to_set;
  int t = (inside + RAMP_MIN / 2) * 256 / RAMP_MIN;
  return clamp(t, 0, 256);
}

bool sky_sun_height(const struct tm *now, uint8_t *flame_height) {
  if (!(s_sky.has & HAS_SUN)) {
    return false;
  }
  int day = daylight(now->tm_hour * 60 + now->tm_min, s_sky.sunrise, s_sky.sunset);
  *flame_height = (NIGHT_HEIGHT * (256 - day) + DAY_HEIGHT * day + 128) / 256;
  return true;
}
