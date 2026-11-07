// Hearth: the time in four large digits, each a window onto a live fire.

#include <pebble.h>

#include "backlight.h"
#include "crackle.h"
#include "fire.h"
#include "flames.h"
#include "layout.h"
#include "settings.h"
#include "sky.h"

#define IDLE_TIMEOUT_MS 30000
// Heat added to every cell when the digits flare on the minute, and when a
// tap or shake stokes the fire. Doom's fire burns it down in a few seconds.
#define FLARE_HEAT 8
#define STOKE_HEAT 10
// How often to ask the phone for weather and sun times, in minutes.
#define SKY_EVERY_MIN 30
// Tilt: accelerometer x (milli-g) per unit of wind.
#define TILT_PER_WIND 350
// Log the average frame cost once, after this many frames.
#define PROFILE_FRAMES 100

static const uint32_t s_digit_resources[2][10] = {
  [DigitSetLarge] = {
    RESOURCE_ID_DIGIT_0, RESOURCE_ID_DIGIT_1, RESOURCE_ID_DIGIT_2, RESOURCE_ID_DIGIT_3,
    RESOURCE_ID_DIGIT_4, RESOURCE_ID_DIGIT_5, RESOURCE_ID_DIGIT_6, RESOURCE_ID_DIGIT_7,
    RESOURCE_ID_DIGIT_8, RESOURCE_ID_DIGIT_9,
  },
  [DigitSetCompact] = {
    RESOURCE_ID_DIGIT_S_0, RESOURCE_ID_DIGIT_S_1, RESOURCE_ID_DIGIT_S_2, RESOURCE_ID_DIGIT_S_3,
    RESOURCE_ID_DIGIT_S_4, RESOURCE_ID_DIGIT_S_5, RESOURCE_ID_DIGIT_S_6, RESOURCE_ID_DIGIT_S_7,
    RESOURCE_ID_DIGIT_S_8, RESOURCE_ID_DIGIT_S_9,
  },
};

static Window *s_window;
static Layer *s_canvas;

static GBitmap *s_digits[4];
static uint8_t s_digit_values[4] = {0xFF, 0xFF, 0xFF, 0xFF};  // loaded bitmaps
static uint8_t s_time_digits[4];                                // the time to show
static DigitSet s_digit_set = DigitSetLarge;
static Layout s_layout;

static Fire *s_fire;
static uint8_t s_cell_shift;   // cell size shift the current fire was built for
static int16_t s_fire_window_w; // window width the current fire was built for

static Settings s_settings;
static bool s_settings_pending;  // arrived while unfocused, apply on refocus

static AppTimer *s_frame_timer;
static AppTimer *s_idle_timer;
static bool s_focused = true;
static bool s_awake = true;      // Animate: always true, or true for 30 s after a tap
static bool s_running;           // focused && awake: frames are being produced
#if defined(PBL_COLOR)
static bool s_tilt_subscribed;
#endif

static uint32_t s_profile_frames;
static uint32_t s_profile_step_ms;
static uint32_t s_profile_draw_ms;

// ---------------------------------------------------------------------------
// Fire lifetime

static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000 + ms;
}

static uint8_t wanted_cell_shift(void) {
#if defined(PBL_COLOR)
  return s_settings.resolution;
#else
  return 2;  // B&W always dithers 4 px cells
#endif
}

// The flame height comes from the setting, the outside temperature or the
// sun, falling back to the setting until the phone has sent the values.
static uint8_t flame_height(void) {
  int8_t wind;
  uint8_t height;
  if (s_settings.height_source == HeightWeather && sky_weather(&wind, &height)) {
    return height;
  }
  if (s_settings.height_source == HeightSun) {
    time_t now = time(NULL);
    if (sky_sun_height(localtime(&now), &height)) {
      return height;
    }
  }
  return s_settings.flame_height;
}

#if defined(PBL_COLOR)
static int8_t base_wind(void) {
  int8_t wind;
  uint8_t height;
  if (s_settings.weather_wind && sky_weather(&wind, &height)) {
    return wind;
  }
  return s_settings.wind;
}
#endif

// What the sky settings need from the phone (SKY_NEED_* bits).
static uint8_t sky_needs(void) {
  uint8_t needs = 0;
  if (s_settings.height_source == HeightSun) needs |= SKY_NEED_SUN;
  if (s_settings.height_source == HeightWeather) needs |= SKY_NEED_WEATHER;
#if defined(PBL_COLOR)
  if (s_settings.weather_wind) needs |= SKY_NEED_WEATHER;
#endif
  return needs;
}

static void configure_fire(void) {
  if (!s_fire) {
    return;
  }
  fire_set_reach(s_fire, settings_top_heat(flame_height()));
#if defined(PBL_COLOR)
  fire_set_wind(s_fire, base_wind());
#endif
}

static Fire *create_fire(uint16_t window_w) {
  for (uint8_t shift = wanted_cell_shift(); shift <= 2; shift++) {
    uint16_t cell = 1 << shift;
    Fire *fire = fire_create((window_w + cell - 1) >> shift, rand());
    if (fire) {
      s_cell_shift = shift;
      return fire;
    }
  }
  return NULL;
}

// Replaces the fire with one for the current window width and resolution.
// (Its rows do not depend on the window: they are stretched over whatever
// height the digits take.) The new fire is made while the old one still
// exists, so if the heap cannot fit it at the requested size or any coarser
// one, the face keeps burning with the old fire. Only if that fails too is
// the old one freed to make room for a retry; a fine grid that fits on its
// own but not next to the old one still gets made that way.
static void rebuild_fire(void) {
  uint16_t window_w = s_layout.window.size.w;
  uint8_t old_shift = s_cell_shift;
  Fire *fire = create_fire(window_w);
  if (!fire && s_fire) {
    fire_destroy(s_fire);
    s_fire = NULL;
    fire = create_fire(window_w);
  }
  if (fire) {
    fire_destroy(s_fire);
    s_fire = fire;
    configure_fire();
    APP_LOG(APP_LOG_LEVEL_INFO, "fire %d px cells, heap free %d",
            1 << s_cell_shift, (int)heap_bytes_free());
  } else if (s_fire) {
    s_cell_shift = old_shift;
    APP_LOG(APP_LOG_LEVEL_WARNING, "no memory for a new fire; keeping the old one");
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "no memory for the fire");
  }
  // Remember the width even on failure, so the minute tick does not retry
  // (and log) every minute; a settings change retries.
  s_fire_window_w = window_w;
}

// ---------------------------------------------------------------------------
// Digits and layout

static void set_digit(int index, uint8_t value) {
  if (s_digit_values[index] == value) {
    return;  // only changed digits are reloaded
  }
  GBitmap *bitmap = gbitmap_create_with_resource(s_digit_resources[s_digit_set][value]);
  if (!bitmap) {
    return;  // keep the old digit rather than show a hole
  }
  gbitmap_destroy(s_digits[index]);
  s_digits[index] = bitmap;
  s_digit_values[index] = value;
}

// Picks the digit set for the visible area (large, or compact while Timeline
// Quick View covers the bottom of the screen), loads the time's digits in it,
// and lays them out.
static void update_layout(void) {
  GRect area = layer_get_unobstructed_bounds(s_canvas);
  DigitSet set = layout_choose_set(area.size.h);
  if (set != s_digit_set) {
    s_digit_set = set;
    for (int i = 0; i < 4; i++) {
      s_digit_values[i] = 0xFF;  // force a reload in the new set
    }
  }
  for (int i = 0; i < 4; i++) {
    set_digit(i, s_time_digits[i]);
  }
  GSize sizes[4];
  for (int i = 0; i < 4; i++) {
    sizes[i] = s_digits[i] ? gbitmap_get_bounds(s_digits[i]).size : GSize(0, 0);
  }
  GRect bounds = layer_get_bounds(s_canvas);
  layout_compute(&s_layout, s_digit_set, area, bounds.size.w, sizes);
  if (s_layout.window.size.w != s_fire_window_w) {
    rebuild_fire();
  }
}

static void update_time(struct tm *t) {
  int hour = t->tm_hour;
  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) hour = 12;
  }
  // Always four digits ("0907"), so the layout never jumps.
  s_time_digits[0] = hour / 10;
  s_time_digits[1] = hour % 10;
  s_time_digits[2] = t->tm_min / 10;
  s_time_digits[3] = t->tm_min % 10;
  update_layout();  // loads changed digits (all of them if the set changed)
  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Drawing

static void canvas_update(Layer *layer, GContext *ctx) {
  uint32_t start = now_ms();
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  for (int i = 0; i < 4; i++) {
    if (s_digits[i]) {
      graphics_draw_bitmap_in_rect(ctx, s_digits[i], s_layout.digits[i]);
    }
  }
  if (!s_fire) {
    return;  // plain white digits rather than nothing
  }
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    flames_render(fb, s_fire, s_layout.window, s_cell_shift);
    graphics_release_frame_buffer(ctx, fb);
  }
  if (s_running && s_profile_frames < PROFILE_FRAMES) {
    s_profile_draw_ms += now_ms() - start;
  }
}

// ---------------------------------------------------------------------------
// Animation

static void update_tilt_subscription(void) {
#if defined(PBL_COLOR)
  bool want = s_running && s_settings.tilt_wind;
  if (want && !s_tilt_subscribed) {
    // Subscribing with no handler keeps the accelerometer running for peeks.
    accel_data_service_subscribe(0, NULL);
  } else if (!want && s_tilt_subscribed) {
    accel_data_service_unsubscribe();
    if (s_fire) fire_set_wind(s_fire, base_wind());
  }
  s_tilt_subscribed = want;
#endif
}

static void frame(void *context) {
  // Schedule first so the frame rate does not drift by the cost of a frame.
  s_frame_timer = app_timer_register(1000 / s_settings.fps, frame, NULL);
  if (!s_fire) {
    return;
  }
  uint32_t start = now_ms();
#if defined(PBL_COLOR)
  if (s_tilt_subscribed) {
    AccelData accel;
    if (accel_service_peek(&accel) == 0) {
      fire_set_wind(s_fire, base_wind() + accel.x / TILT_PER_WIND);
    }
  }
#endif
  fire_step(s_fire);
  layer_mark_dirty(s_canvas);
#if defined(PBL_RGB_BACKLIGHT)
  if (s_settings.backlight) {
    backlight_update(s_fire, s_settings.flicker);
  }
#endif
  if (s_profile_frames < PROFILE_FRAMES) {
    s_profile_step_ms += now_ms() - start;
    if (++s_profile_frames == PROFILE_FRAMES) {
      APP_LOG(APP_LOG_LEVEL_INFO, "per frame: step %d.%02d ms, draw %d.%02d ms, heap free %d",
              (int)(s_profile_step_ms / PROFILE_FRAMES), (int)(s_profile_step_ms % PROFILE_FRAMES),
              (int)(s_profile_draw_ms / PROFILE_FRAMES), (int)(s_profile_draw_ms % PROFILE_FRAMES),
              (int)heap_bytes_free());
    }
  }
}

#if defined(PBL_SPEAKER)
static void start_sound(void) {
  if (s_settings.crackle) {
    crackle_start(s_settings.volume);
  } else {
    crackle_stop();
  }
}
#endif

// Starts or stops everything that runs per frame: the frame timer, the
// backlight, the crackle and the tilt sensor. Frozen means the last frame
// stays on screen and nothing wakes the CPU but the minute tick.
static void update_running(void) {
  bool run = s_focused && s_awake;
  if (run != s_running) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, run ? "animating" : "frozen");
  }
  if (run && !s_running) {
    s_running = true;
    if (!s_frame_timer) {
      s_frame_timer = app_timer_register(1000 / s_settings.fps, frame, NULL);
    }
  } else if (!run && s_running) {
    s_running = false;
    if (s_frame_timer) {
      app_timer_cancel(s_frame_timer);
      s_frame_timer = NULL;
    }
  }
#if defined(PBL_RGB_BACKLIGHT)
  if (!s_running || !s_settings.backlight) {
    backlight_release();
  }
#endif
#if defined(PBL_SPEAKER)
  if (s_running) {
    start_sound();
  } else {
    crackle_stop();
  }
#endif
  update_tilt_subscription();
}

// The flames leap at once, then Doom's fire burns the extra heat down.
static void flare(uint8_t heat) {
  if (s_running && s_fire) {
    fire_stoke(s_fire, heat);
  }
}

static void idle_timeout(void *context) {
  s_idle_timer = NULL;
  s_awake = false;
  update_running();
}

static void wake(void) {
  s_awake = true;
  if (s_idle_timer) {
    app_timer_reschedule(s_idle_timer, IDLE_TIMEOUT_MS);
  } else {
    s_idle_timer = app_timer_register(IDLE_TIMEOUT_MS, idle_timeout, NULL);
  }
  update_running();
}

// A tap wakes a frozen face (Animate: after a tap) and stokes the fire.
static void on_tap(void) {
  if (s_settings.animate == AnimateOnTap) {
    wake();
  }
  if (s_settings.stoke) {
    flare(STOKE_HEAT);
  }
}

// The accelerometer's tap: a sharp flick of the wrist or knock on the watch.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  on_tap();
}

#if defined(PBL_TOUCH)
// On a touchscreen (Pebble Time 2, Round 2) a finger on the screen is the
// natural tap; the accelerometer does not register it.
static void touch_handler(const TouchEvent *event, void *context) {
  if (event->type == TouchEvent_Touchdown) {
    on_tap();
  }
}
#endif

// ---------------------------------------------------------------------------
// Settings

static void apply_settings(void) {
  static bool s_tap_subscribed;
  flames_configure(&s_settings);
  if (!s_fire || wanted_cell_shift() != s_cell_shift) {
    rebuild_fire();
  } else {
    configure_fire();
  }

  // Taps wake the face (Animate: On tap) and stoke it. The tap service is
  // event driven, so listening costs no battery between taps.
  bool want_taps = s_settings.animate == AnimateOnTap || s_settings.stoke;
  if (want_taps && !s_tap_subscribed) {
    accel_tap_service_subscribe(tap_handler);
#if defined(PBL_TOUCH)
    touch_service_subscribe(touch_handler, NULL);
#endif
    s_tap_subscribed = true;
  } else if (!want_taps && s_tap_subscribed) {
    accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
#if defined(PBL_TOUCH)
    touch_service_unsubscribe();
#endif
    s_tap_subscribed = false;
  }
  // Tap mode starts awake so the change is visible at once.
  if (s_settings.animate == AnimateOnTap) {
    if (!s_idle_timer) {
      wake();
    }
  } else {
    if (s_idle_timer) {
      app_timer_cancel(s_idle_timer);
      s_idle_timer = NULL;
    }
    s_awake = true;
  }

  // A new frame rate takes effect on the next frame; restart the timer so a
  // slow rate does not delay it.
  if (s_frame_timer) {
    app_timer_reschedule(s_frame_timer, 1000 / s_settings.fps);
  }
  // Backlight, crackle and tilt are re-applied on every call.
  update_running();
  layer_mark_dirty(s_canvas);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool sky = sky_from_message(iter);
#if defined(PBL_SPEAKER)
  uint8_t old_crackle = s_settings.crackle, old_volume = s_settings.volume;
#endif
  bool settings = settings_from_message(&s_settings, iter);
#if defined(PBL_SPEAKER)
  // Turning the crackle on or changing its volume plays one pop at once, so
  // the level can be heard while it is being set.
  if (settings && s_settings.crackle &&
      (s_settings.crackle != old_crackle || s_settings.volume != old_volume)) {
    crackle_preview(s_settings.volume);
  }
#endif
  if (settings) {
    settings_save(&s_settings);
    if (s_focused) {
      apply_settings();
    } else {
      s_settings_pending = true;
    }
    // Settings that now use the sky get their first values without waiting
    // for the half-hour tick.
    sky_request(sky_needs());
  } else if (sky) {
    // Weather and sun times only change the fire's height and wind. They
    // must not go through apply_settings, which would wake a face that is
    // frozen (Animate: On tap) every half hour, backlight and crackle too.
    configure_fire();
    layer_mark_dirty(s_canvas);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "settings message dropped: %d", (int)reason);
}

// ---------------------------------------------------------------------------
// System events

static void tick_handler(struct tm *t, TimeUnits changed) {
  update_time(t);
  if (s_settings.flare) {
    flare(FLARE_HEAT);
  }
  if (t->tm_min % SKY_EVERY_MIN == 0) {
    sky_request(sky_needs());
  }
  if (s_settings.height_source == HeightSun) {
    configure_fire();  // follow the sun through dusk and dawn
  }
}

static void focus_handler(bool in_focus) {
  s_focused = in_focus;
  if (in_focus && s_settings_pending) {
    s_settings_pending = false;
    apply_settings();
  }
  update_running();
  if (in_focus) {
    layer_mark_dirty(s_canvas);  // the notification left the digits unpainted
  }
}

#if !defined(PBL_PLATFORM_APLITE)
static void unobstructed_did_change(void *context) {
  update_layout();
  layer_mark_dirty(s_canvas);
}
#endif

// ---------------------------------------------------------------------------
// Lifecycle

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  time_t now = time(NULL);
  update_time(localtime(&now));  // also lays out and builds the fire
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

// The phone's JavaScript is not ready the moment the face starts, so ask for
// sky values a few seconds in.
static void first_sky_request(void *context) {
  sky_request(sky_needs());
}

static void init(void) {
  srand(time(NULL));
  settings_load(&s_settings);
  sky_load();
  flames_configure(&s_settings);
#if defined(PBL_SPEAKER)
  crackle_init();
#endif
  s_cell_shift = wanted_cell_shift();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, false);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = focus_handler,
  });
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers){
    .did_change = unobstructed_did_change,
  }, NULL);
#endif

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  // 23 int32 keys (16 settings, 6 sky values, 1 request) at about 11 bytes
  // each, plus the dictionary header.
  const uint32_t inbox = 23 * 11 + 16;
  if (app_message_open(inbox, 64) != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_open failed; settings unavailable");
  }

  apply_settings();
  app_timer_register(5000, first_sky_request, NULL);
  APP_LOG(APP_LOG_LEVEL_INFO, "started, heap free %d", (int)heap_bytes_free());
}

static void deinit(void) {
  s_focused = false;
  update_running();
  if (s_idle_timer) app_timer_cancel(s_idle_timer);
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  app_focus_service_unsubscribe();
  window_destroy(s_window);
  for (int i = 0; i < 4; i++) {
    gbitmap_destroy(s_digits[i]);
  }
  fire_destroy(s_fire);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
