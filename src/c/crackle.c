#include "crackle.h"

#if defined(PBL_SPEAKER)

#include <stdlib.h>

#include "crackle_synth.h"

#define BASE_NOTE 60

// Each crackle is synthesised afresh just before it plays (only when the
// speaker is idle, so the buffer is never rewritten mid-play) and played at
// its own pitch: no two sound the same.
static int8_t s_sample_data[CRACKLE_MAX_SAMPLES];
static SpeakerSample s_sample = {
  .data = s_sample_data,
  .num_bytes = CRACKLE_MAX_SAMPLES,
  .format = SpeakerPcmFormat_16kHz_8bit,
  .base_midi_note = BASE_NOTE,
  .loop = false,
};
static uint32_t s_rng;
static SpeakerNote s_note;
static const SpeakerTrack s_track = {
  .notes = &s_note,
  .num_notes = 1,
  .sample = &s_sample,
};

static AppTimer *s_timer;
static uint8_t s_volume;

// Why the last pop did not play, so it is logged once rather than per pop.
typedef enum { SilentNo = 0, SilentMuted, SilentQuietTime, SilentFailed } Silent;
static Silent s_silent;

void crackle_init(void) {
  s_rng = (uint32_t)rand() | 1;
}

static void report(Silent why) {
  static const char *const reasons[] = {
    "", "the watch speaker is muted (Settings > Sounds)", "Quiet Time is on", "the speaker refused to play",
  };
  if (why != s_silent && why != SilentNo) {
    APP_LOG(APP_LOG_LEVEL_INFO, "crackle silent: %s", reasons[why]);
  }
  s_silent = why;
}

// Plays one pop at volume 1..10. Respects the user's mute and Quiet Time,
// and never cuts into other sound or our own previous pop.
static void pop(uint8_t volume) {
  if (speaker_is_muted()) {
    report(SilentMuted);
    return;
  }
  if (quiet_time_is_active()) {
    report(SilentQuietTime);
    return;
  }
  if (speaker_get_status() != SpeakerStatusIdle) {
    return;
  }
  uint16_t len = crackle_synth(s_sample_data, &s_rng);
  s_sample.num_bytes = len;
  s_note = (SpeakerNote){
    .midi_note = BASE_NOTE,
    .waveform = SpeakerWaveformSine,
    .duration_ms = (uint16_t)(len * 1000 / CRACKLE_RATE + 5),
    .velocity = 80 + rand() % 48,
  };
  // Volume 1..10 is 10..100 % of the speaker. (The first version used 2..20 %,
  // which with a short pop was too quiet to hear on a watch.)
  bool ok = speaker_play_tracks(&s_track, 1, volume * 10);
  report(ok ? SilentNo : SilentFailed);
}

static void schedule(void);

static void timer_fired(void *ctx) {
  s_timer = NULL;
  pop(s_volume);
  schedule();
}

// Real fires crackle in clusters: mostly long gaps, a third of the time a
// quick follow-up pop.
static void schedule(void) {
  uint32_t ms = (rand() % 3 == 0) ? 80 + rand() % 121 : 400 + rand() % 2201;
  s_timer = app_timer_register(ms, timer_fired, NULL);
}

void crackle_start(uint8_t volume) {
  if (volume == 0) {
    crackle_stop();
    return;
  }
  s_volume = volume;
  if (!s_timer) {
    schedule();
  }
}

void crackle_stop(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
    speaker_stop();
  }
}

void crackle_preview(uint8_t volume) {
  pop(volume);
}

#endif
