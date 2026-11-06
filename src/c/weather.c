#include "weather.h"

#define WEATHER_KEY 2
#define WEATHER_VERSION 1

typedef struct __attribute__((__packed__)) {
  uint8_t version;
  int8_t wind;
  uint8_t flame_height;
} Weather;

static Weather s_weather;
static bool s_valid;

void weather_load(void) {
  Weather stored;
  s_valid = persist_read_data(WEATHER_KEY, &stored, sizeof(stored)) == (int)sizeof(stored) &&
            stored.version == WEATHER_VERSION;
  if (s_valid) {
    s_weather = stored;
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

bool weather_from_message(DictionaryIterator *iter) {
  int32_t wind, height;
  // Both arrive together; ignore a half message rather than mix old and new.
  if (!read_int(iter, MESSAGE_KEY_WeatherWind, &wind) ||
      !read_int(iter, MESSAGE_KEY_WeatherHeight, &height)) {
    return false;
  }
  s_weather = (Weather){
    .version = WEATHER_VERSION,
    .wind = wind < -2 ? -2 : (wind > 2 ? 2 : wind),
    .flame_height = height < 1 ? 1 : (height > 10 ? 10 : height),
  };
  s_valid = true;
  persist_write_data(WEATHER_KEY, &s_weather, sizeof(s_weather));
  return true;
}

void weather_request(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;  // busy or no phone: the next half-hour tick tries again
  }
  dict_write_int8(iter, MESSAGE_KEY_WeatherRequest, 1);
  app_message_outbox_send();
}

bool weather_get(int8_t *wind, uint8_t *flame_height) {
  if (!s_valid) {
    return false;
  }
  *wind = s_weather.wind;
  *flame_height = s_weather.flame_height;
  return true;
}
