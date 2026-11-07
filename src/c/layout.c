#include "layout.h"
#include "generated/metrics.h"

typedef struct {
  int height, max_pair_width, col_gap, row_gap;
} Metrics;

static const Metrics s_metrics[2] = {
  [DigitSetLarge] = {DIGIT_L_HEIGHT, DIGIT_L_MAX_PAIR_WIDTH, DIGIT_L_COL_GAP, DIGIT_L_ROW_GAP},
  [DigitSetCompact] = {DIGIT_S_HEIGHT, DIGIT_S_MAX_PAIR_WIDTH, DIGIT_S_COL_GAP, DIGIT_S_ROW_GAP},
};

DigitSet layout_choose_set(int area_h) {
  const Metrics *m = &s_metrics[DigitSetLarge];
  return area_h >= 2 * m->height + m->row_gap ? DigitSetLarge : DigitSetCompact;
}

void layout_compute(Layout *out, DigitSet set, GRect area, int screen_w,
                    const GSize digit_sizes[4]) {
  const Metrics *m = &s_metrics[set];
  // When space is still short, give up the row gap first.
  int row_gap = m->row_gap;
  int spare = area.size.h - 2 * m->height;
  if (row_gap > spare) {
    row_gap = spare > 0 ? spare : 0;
  }
  int block_h = 2 * m->height + row_gap;
  int top = area.origin.y + (area.size.h - block_h) / 2;

  for (int row = 0; row < 2; row++) {
    GSize a = digit_sizes[row * 2], b = digit_sizes[row * 2 + 1];
    int row_w = a.w + m->col_gap + b.w;
    int x = (screen_w - row_w) / 2;
    int y = top + row * (m->height + row_gap);
    out->digits[row * 2] = GRect(x, y, a.w, m->height);
    out->digits[row * 2 + 1] = GRect(x + a.w + m->col_gap, y, b.w, m->height);
  }

  // Every row is at most max_pair_width + gap wide and centered the same
  // way, so this box always contains both rows.
  int win_w = m->max_pair_width + m->col_gap;
  int left = (screen_w - win_w) / 2;
  int right = left + win_w;
#if defined(PBL_BW)
  // The B&W renderer works a byte (8 pixels, two 4 px cells) at a time.
  left &= ~7;
  right = (right + 7) & ~7;
  if (left < 0) left = 0;
  if (right > screen_w) right = screen_w & ~7;
#endif
  out->window = GRect(left, top, right - left, block_h);
}
