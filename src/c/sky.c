#include "sky.h"

#include "daylight.h"
#include "settings.h"

#define SKY_KEY 3
// 2: sunrise and sunset became signed and may fall outside today.
#define SKY_VERSION 2

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
  int16_t sunrise;        // minutes after local midnight, -1440..1440
  int16_t sunset;         // 0..2880; equal to sunrise: no sunrise today
} Sky;

static Sky s_sky;

void sky_load(void) {
  Sky stored;
  if (persist_read_data(SKY_KEY, &stored, sizeof(stored)) == (int)sizeof(stored) &&
      stored.version == SKY_VERSION) {
    s_sky = stored;
  }
}

static int clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

bool sky_from_message(DictionaryIterator *iter) {
  bool any = false;
  int32_t a, b;
  // Pairs arrive together; ignore half a pair rather than mix old and new.
  if (settings_read_int(iter, MESSAGE_KEY_SkyWind, &a) &&
      settings_read_int(iter, MESSAGE_KEY_SkyHeight, &b)) {
    s_sky.wind = clamp(a, -2, 2);
    s_sky.flame_height = clamp(b, 1, 10);
    s_sky.has |= HAS_WEATHER;
    any = true;
  }
  if (settings_read_int(iter, MESSAGE_KEY_SunRise, &a) &&
      settings_read_int(iter, MESSAGE_KEY_SunSet, &b)) {
    s_sky.sunrise = clamp(a, -1440, 1440);
    s_sky.sunset = clamp(b, 0, 2880);
    s_sky.has |= HAS_SUN;
    any = true;
  }
  if (any) {
    s_sky.version = SKY_VERSION;
    persist_write_data(SKY_KEY, &s_sky, sizeof(s_sky));
  }
  return any;
}

void sky_request(uint8_t needs) {
  DictionaryIterator *iter;
  if (!needs || app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;  // nothing needed, or busy or no phone: the next request tries again
  }
  dict_write_int8(iter, MESSAGE_KEY_SkyRequest, needs);
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

bool sky_sun_height(const struct tm *now, uint8_t *flame_height) {
  if (!(s_sky.has & HAS_SUN)) {
    return false;
  }
  int day = daylight(now->tm_hour * 60 + now->tm_min, s_sky.sunrise, s_sky.sunset, RAMP_MIN);
  *flame_height = (NIGHT_HEIGHT * (DAYLIGHT_FULL - day) + DAY_HEIGHT * day + DAYLIGHT_FULL / 2) /
                  DAYLIGHT_FULL;
  return true;
}
