#include "flames.h"

#if defined(PBL_COLOR)

typedef struct {
  uint8_t heat;
  uint32_t rgb;
} Stop;

#define END {0xFF, 0}

static const Stop s_classic[] = {
  {0, 0x000000}, {8, 0x550000}, {15, 0xAA0000}, {23, 0xFF5500},
  {30, 0xFFAA00}, {34, 0xFFFF55}, {36, 0xFFFFFF}, END,
};
// Ember: the classic ramp without yellow, topping out at a pale orange.
static const Stop s_ember[] = {
  {0, 0x000000}, {8, 0x550000}, {16, 0xAA0000}, {26, 0xFF0000},
  {32, 0xFF5500}, {36, 0xFFAA55}, END,
};
static const Stop s_blue[] = {
  {0, 0x000000}, {8, 0x000055}, {15, 0x0000AA}, {23, 0x0055FF},
  {30, 0x00AAFF}, {34, 0x55FFFF}, {36, 0xFFFFFF}, END,
};
static const Stop s_green[] = {
  {0, 0x000000}, {8, 0x005500}, {15, 0x00AA00}, {23, 0x55AA00},
  {30, 0x55FF00}, {34, 0xAAFF55}, {36, 0xFFFFFF}, END,
};
static const Stop s_purple[] = {
  {0, 0x000000}, {8, 0x550055}, {15, 0x5500AA}, {23, 0xAA00FF},
  {30, 0xFF55FF}, {34, 0xFFAAFF}, {36, 0xFFFFFF}, END,
};

static GColor8 s_palette[FIRE_PALETTE_SIZE];

static uint32_t argb8_to_rgb(uint8_t argb) {
  GColor8 c = (GColor8){.argb = argb};
  return (c.r * 85u) << 16 | (c.g * 85u) << 8 | (c.b * 85u);
}

// Blending between Pebble-exact stops lands between the 4 levels per channel;
// round to the nearest one rather than truncating, which would darken the ramp.
static uint8_t blend(uint32_t a, uint32_t b, int shift, int t, int span) {
  int ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
  int v = ca + (cb - ca) * t / span;
  return (v + 42) / 85 * 85;
}

static void build_palette(const Stop *stops) {
  for (int i = 0; stops[i + 1].heat != 0xFF; i++) {
    const Stop *a = &stops[i], *b = &stops[i + 1];
    int span = b->heat - a->heat;
    for (int h = a->heat; h <= b->heat; h++) {
      int t = h - a->heat;
      s_palette[h] = GColorFromRGB(blend(a->rgb, b->rgb, 16, t, span),
                                   blend(a->rgb, b->rgb, 8, t, span),
                                   blend(a->rgb, b->rgb, 0, t, span));
    }
  }
}

void flames_configure(const Settings *s) {
  static const Stop *themes[] = {s_classic, s_ember, s_blue, s_green, s_purple};
  if (s->theme == ThemeCustom) {
    Stop custom[] = {
      {0, 0x000000},
      {10, argb8_to_rgb(s->custom[0])},
      {22, argb8_to_rgb(s->custom[1])},
      {32, argb8_to_rgb(s->custom[2])},
      {36, 0xFFFFFF},
      END,
    };
    build_palette(custom);
  } else {
    build_palette(themes[s->theme < ThemeCustom ? s->theme : ThemeClassic]);
  }
}

void flames_render(GBitmap *fb, const Fire *fire, GRect window, uint8_t shift) {
  GRect fb_bounds = gbitmap_get_bounds(fb);
  int top = window.origin.y, left = window.origin.x;
  int y0 = top > 0 ? top : 0;
  int y1 = top + window.size.h;
  if (y1 > fb_bounds.size.h) y1 = fb_bounds.size.h;
  // Doom's rows are stretched over the whole window height.
  int rows = fire_height(fire);
  int win_h = window.size.h > 0 ? window.size.h : 1;
  for (int y = y0; y < y1; y++) {
    // Row info gives the real x range of each row, which matters on round
    // screens where rows are shorter than the display width.
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    int x0 = left > info.min_x ? left : info.min_x;
    int x1 = left + window.size.w - 1;
    if (x1 > info.max_x) x1 = info.max_x;
    int cy = (y - top) * rows / win_h;
    const uint8_t *cells = fire_row(fire, cy);
    uint8_t *px = info.data;
    for (int x = x0; x <= x1; x++) {
      if (px[x] == GColorWhiteARGB8) px[x] = s_palette[cells[(x - left) >> shift]].argb;
    }
  }
}

#else  // PBL_BW

// One nibble per pattern row, bit n = pixel n of the 4 px cell (the 1-bit
// framebuffer stores the leftmost pixel in the least significant bit).
static uint8_t s_dither[FIRE_PALETTE_SIZE][4];

// 4x4 Bayer matrix: thresholds 0..15 spread so any level looks even.
static const uint8_t s_bayer[4][4] = {
  {0, 8, 2, 10},
  {12, 4, 14, 6},
  {3, 11, 1, 9},
  {15, 7, 13, 5},
};

void flames_configure(const Settings *s) {
  (void)s;
  for (int h = 0; h <= FIRE_MAX_HEAT; h++) {
    // Lit pixels out of 16. A ramp from heat 6 (nothing) to 30 (solid),
    // tuned in the emulator: a linear ramp over 0..36 reads as a halftone
    // gradient, while this gives a solid hot base and broken, flickering tips.
    int level = (h - 6) * 16 / 24;
    if (level < 0) level = 0;
    if (level > 16) level = 16;
    for (int py = 0; py < 4; py++) {
      uint8_t nibble = 0;
      for (int px = 0; px < 4; px++) {
        if (s_bayer[py][px] < level) nibble |= 1 << px;
      }
      s_dither[h][py] = nibble;
    }
  }
}

void flames_render(GBitmap *fb, const Fire *fire, GRect window, uint8_t shift) {
  (void)shift;  // always 4 px cells
  if (gbitmap_get_format(fb) != GBitmapFormat1Bit) {
    return;
  }
  GRect fb_bounds = gbitmap_get_bounds(fb);
  uint16_t bpr = gbitmap_get_bytes_per_row(fb);
  uint8_t *data = gbitmap_get_data(fb);
  int top = window.origin.y;
  int first_byte = window.origin.x >> 3;
  int bytes = window.size.w >> 3;
  if (first_byte < 0 || (first_byte + bytes) > bpr) {
    return;
  }
  int y0 = top > 0 ? top : 0;
  int y1 = top + window.size.h;
  if (y1 > fb_bounds.size.h) y1 = fb_bounds.size.h;
  // Doom's rows are stretched over the whole window height.
  int rows = fire_height(fire);
  int win_h = window.size.h > 0 ? window.size.h : 1;
  for (int y = y0; y < y1; y++) {
    int cy = (y - top) * rows / win_h;
    const uint8_t *cells = fire_row(fire, cy);
    int py = (y - top) & 3;
    uint8_t *row = data + y * bpr + first_byte;
    for (int b = 0; b < bytes; b++) {
      if (!row[b]) continue;  // all black: nothing to color
      // White stays white only where the pattern bit is 1.
      row[b] &= s_dither[cells[2 * b]][py] | (s_dither[cells[2 * b + 1]][py] << 4);
    }
  }
}

#endif
