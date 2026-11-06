// Hearth: the time in four large digits, each a window onto a live fire.

#include <pebble.h>

#include "backlight.h"
#include "crackle.h"
#include "fire.h"
#include "flames.h"
#include "layout.h"
#include "settings.h"
#include "weather.h"

#define IDLE_TIMEOUT_MS 30000
// Extra reach while the digits flare on the minute, and for how long.
#define FLARE_FUEL 12
#define FLARE_HEAT 8
#define FLARE_MS 1000
// A tap or shake stokes the fire the same way, a little longer.
#define STOKE_MS 1500
// How often to ask the phone for weather, in minutes past the hour.
#define WEATHER_EVERY_MIN 30
// Tilt: accelerometer x (milli-g) per unit of wind.
#define TILT_PER_WIND 350
// Log the average frame cost once, after this many frames.
#define PROFILE_FRAMES 100

static const uint32_t s_digit_resources[10] = {
  RESOURCE_ID_DIGIT_0, RESOURCE_ID_DIGIT_1, RESOURCE_ID_DIGIT_2, RESOURCE_ID_DIGIT_3,
  RESOURCE_ID_DIGIT_4, RESOURCE_ID_DIGIT_5, RESOURCE_ID_DIGIT_6, RESOURCE_ID_DIGIT_7,
  RESOURCE_ID_DIGIT_8, RESOURCE_ID_DIGIT_9,
};

#if defined(PBL_COLOR)
static const int8_t s_turbulence_tables[TurbulenceCount][4] = {
  {0, 0, 0, 0},
  {-1, 0, 0, 1},
  {-2, -1, 1, 2},
};
#endif

static Window *s_window;
static Layer *s_canvas;

static GBitmap *s_digits[4];
static uint8_t s_digit_values[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static Layout s_layout;

static Fire *s_fire;
static uint8_t s_cell_shift;   // cell size shift the current fire was built for
static GSize s_fire_window;    // window size the current fire was built for

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
static uint16_t s_flare_frames;

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

// With the Weather setting on (and weather received), the local weather
// replaces the Flame height and Wind settings.
static uint8_t flame_height(void) {
  int8_t wind;
  uint8_t height;
  if (s_settings.weather && weather_get(&wind, &height)) {
    return height;
  }
  return s_settings.flame_height;
}

#if defined(PBL_COLOR)
static int8_t base_wind(void) {
  int8_t wind;
  uint8_t height;
  if (s_settings.weather && weather_get(&wind, &height)) {
    return wind;
  }
  return s_settings.wind;
}
#endif

static void configure_fire(void) {
  if (!s_fire) {
    return;
  }
  fire_set_reach(s_fire, settings_top_heat(flame_height()));
#if defined(PBL_COLOR)
  fire_set_turbulence(s_fire, s_turbulence_tables[s_settings.turbulence]);
  fire_set_wind(s_fire, base_wind());
#endif
  fire_set_fuel(s_fire, s_flare_frames ? FLARE_FUEL : 0);
}

// Destroys and recreates the fire for the current window and resolution.
// The old fire is freed first so a fine grid can use its memory. If the
// requested size does not fit, coarser sizes are tried, and finally the
// previous size, so a failed malloc never leaves the face without flames
// (or crashes it).
static void rebuild_fire(void) {
  uint8_t previous = s_cell_shift;
  fire_destroy(s_fire);
  s_fire = NULL;
  GSize win = s_layout.window.size;
  for (uint8_t shift = wanted_cell_shift(); shift <= 2 && !s_fire; shift++) {
    uint16_t cell = 1 << shift;
    s_fire = fire_create((win.w + cell - 1) >> shift, (win.h + cell - 1) >> shift, rand());
    if (s_fire) s_cell_shift = shift;
  }
  if (!s_fire) {
    uint16_t cell = 1 << previous;
    s_fire = fire_create((win.w + cell - 1) >> previous, (win.h + cell - 1) >> previous, rand());
    if (s_fire) s_cell_shift = previous;
  }
  if (s_fire) {
    s_fire_window = win;
    configure_fire();
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "no memory for the fire");
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "fire %d px cells, heap free %d",
          1 << s_cell_shift, (int)heap_bytes_free());
}

// ---------------------------------------------------------------------------
// Digits and layout

static void update_layout(void) {
  GSize sizes[4];
  for (int i = 0; i < 4; i++) {
    sizes[i] = s_digits[i] ? gbitmap_get_bounds(s_digits[i]).size : GSize(0, 0);
  }
  GRect bounds = layer_get_bounds(s_canvas);
  layout_compute(&s_layout, layer_get_unobstructed_bounds(s_canvas), bounds.size.w, sizes);
  if (!gsize_equal(&s_layout.window.size, &s_fire_window)) {
    rebuild_fire();
  }
}

static void set_digit(int index, uint8_t value) {
  if (s_digit_values[index] == value) {
    return;  // only changed digits are reloaded
  }
  GBitmap *bitmap = gbitmap_create_with_resource(s_digit_resources[value]);
  if (!bitmap) {
    return;  // keep the old digit rather than show a hole
  }
  gbitmap_destroy(s_digits[index]);
  s_digits[index] = bitmap;
  s_digit_values[index] = value;
}

static void update_time(struct tm *t) {
  int hour = t->tm_hour;
  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) hour = 12;
  }
  // Always four digits ("0907"), so the layout never jumps.
  set_digit(0, hour / 10);
  set_digit(1, hour % 10);
  set_digit(2, t->tm_min / 10);
  set_digit(3, t->tm_min % 10);
  update_layout();
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
  if (s_flare_frames && --s_flare_frames == 0) {
    fire_set_fuel(s_fire, 0);
  }
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
    if (s_flare_frames) {
      s_flare_frames = 0;
      if (s_fire) fire_set_fuel(s_fire, 0);
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

// The flames leap at once (stoke), are fed extra fuel for a moment, then
// settle back on their own.
static void flare(uint32_t ms) {
  if (!s_running || !s_fire) {
    return;
  }
  uint16_t frames = (uint32_t)s_settings.fps * ms / 1000;
  if (frames > s_flare_frames) {
    s_flare_frames = frames;
  }
  fire_stoke(s_fire, FLARE_HEAT);
  fire_set_fuel(s_fire, FLARE_FUEL);
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

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_settings.animate == AnimateOnTap) {
    wake();
  }
  if (s_settings.stoke) {
    flare(STOKE_MS);
  }
}

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
    s_tap_subscribed = true;
  } else if (!want_taps && s_tap_subscribed) {
    accel_tap_service_unsubscribe();
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
  bool weather = weather_from_message(iter);
  bool settings = settings_from_message(&s_settings, iter);
  if (!weather && !settings) {
    return;
  }
  if (settings) {
    settings_save(&s_settings);
  }
  if (s_focused) {
    apply_settings();
  } else {
    s_settings_pending = true;
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
    flare(FLARE_MS);
  }
  if (s_settings.weather && t->tm_min % WEATHER_EVERY_MIN == 0) {
    weather_request();
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

static void init(void) {
  srand(time(NULL));
  settings_load(&s_settings);
  weather_load();
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
  // 20 int32 keys (18 settings, 2 weather) at about 11 bytes each, plus the
  // dictionary header.
  const uint32_t inbox = 20 * 11 + 16;
  if (app_message_open(inbox, 64) != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "app_message_open failed; settings unavailable");
  }

  apply_settings();
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
