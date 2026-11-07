#pragma once
// Benchmark helper, not built into the face. To time a face's per-frame work,
// #include this file and call BENCH() from a timer a few seconds after
// launch (so `pebble logs` is attached), e.g.
//   static void bench_cb(void *ctx) { BENCH("fire_step", fire_step(s_fire)); }
//   app_timer_register(3000, bench_cb, NULL);   // in init()
// Run it under tools/bench/qemu-pebble to count CPU work.
//
// Times `body` 200 times back to back, as 8 chunks of 25. Each chunk takes
// well under a second and is timed with the millisecond counter alone
// (mod 1000): on some emulator boards the seconds and milliseconds from
// time_ms are not in step, so a sum across them can jump by a second.
#define BENCH(label, body) do { \
  int32_t total_ms = 0; \
  for (int c_ = 0; c_ < 8; c_++) { \
    uint16_t m0, m1; time_ms(NULL, &m0); \
    for (int i_ = 0; i_ < 25; i_++) { body; } \
    time_ms(NULL, &m1); \
    total_ms += ((int32_t)m1 - (int32_t)m0 + 1000) % 1000; \
  } \
  int32_t us = total_ms * 1000 / 200; \
  APP_LOG(APP_LOG_LEVEL_INFO, "BENCH %s: %d.%03d ms per frame", label, (int)(us / 1000), (int)(us % 1000)); \
} while (0)
