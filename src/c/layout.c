#include "layout.h"
#include "generated/metrics.h"

void layout_compute(Layout *out, GRect area, int screen_w, const GSize digit_sizes[4]) {
  // When space is short (Timeline Quick View), give up the row gap first.
  int row_gap = DIGIT_ROW_GAP;
  int spare = area.size.h - 2 * DIGIT_HEIGHT;
  if (row_gap > spare) {
    row_gap = spare > 0 ? spare : 0;
  }
  int block_h = 2 * DIGIT_HEIGHT + row_gap;
  int top = area.origin.y + (area.size.h - block_h) / 2;

  for (int row = 0; row < 2; row++) {
    GSize a = digit_sizes[row * 2], b = digit_sizes[row * 2 + 1];
    int row_w = a.w + DIGIT_COL_GAP + b.w;
    int x = (screen_w - row_w) / 2;
    int y = top + row * (DIGIT_HEIGHT + row_gap);
    out->digits[row * 2] = GRect(x, y, a.w, DIGIT_HEIGHT);
    out->digits[row * 2 + 1] = GRect(x + a.w + DIGIT_COL_GAP, y, b.w, DIGIT_HEIGHT);
  }

  // Every row is at most DIGIT_MAX_PAIR_WIDTH + gap wide and centered the
  // same way, so this box always contains both rows.
  int win_w = DIGIT_MAX_PAIR_WIDTH + DIGIT_COL_GAP;
  int left = (screen_w - win_w) / 2;
  int right = left + win_w;
#if defined(PBL_BW)
  // The B&W renderer works a byte (8 pixels, two 4 px cells) at a time.
  left &= ~7;
  right = (right + 7) & ~7;
#endif
  out->window = GRect(left, top, right - left, block_h);
}
