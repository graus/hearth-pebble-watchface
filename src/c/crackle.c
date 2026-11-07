#include "crackle.h"

#if defined(PBL_SPEAKER)

#include <stdlib.h>

#define SAMPLE_LEN 480  // 60 ms at 8 kHz
#define BASE_NOTE 60

static int8_t s_sample_data[SAMPLE_LEN];
static const SpeakerSample s_sample = {
  .data = s_sample_data,
  .num_bytes = SAMPLE_LEN,
  .format = SpeakerPcmFormat_8kHz_8bit,
  .base_midi_note = BASE_NOTE,
  .loop = false,
};
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
  // One burst of noise that decays over the 60 ms: a pop, not a hiss. The
  // first version decayed to nothing within about 10 ms, which on a watch
  // speaker is a click too short to hear.
  int32_t amp = 127 << 8;
  for (int i = 0; i < SAMPLE_LEN; i++) {
    int noise = rand() % 255 - 127;
    s_sample_data[i] = (int8_t)(noise * (amp >> 8) / 127);
    amp = amp * 255 / 256;
  }
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
  // Random pitch and loudness make one sample sound like many different pops.
  s_note = (SpeakerNote){
    .midi_note = BASE_NOTE - 12 + rand() % 30,
    .waveform = SpeakerWaveformSine,
    .duration_ms = 60,
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
