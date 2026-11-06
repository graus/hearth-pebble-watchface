#include "crackle.h"

#if defined(PBL_SPEAKER)

#include <stdlib.h>

#define SAMPLE_LEN 320  // 40 ms at 8 kHz
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

void crackle_init(void) {
  // One burst of noise with a fast, roughly exponential decay: a pop, not a hiss.
  int32_t amp = 127 << 8;
  for (int i = 0; i < SAMPLE_LEN; i++) {
    int noise = rand() % 255 - 127;
    s_sample_data[i] = (int8_t)(noise * (amp >> 8) / 127);
    amp = amp * 63 / 64;
  }
}

static void play(void) {
  // Respect the user's mute and Quiet Time, and never cut into other sound
  // (or our own previous pop, which would sound like a click).
  if (speaker_is_muted() || quiet_time_is_active() ||
      speaker_get_status() != SpeakerStatusIdle) {
    return;
  }
  // Random pitch and loudness make one sample sound like many different pops.
  s_note = (SpeakerNote){
    .midi_note = BASE_NOTE - 12 + rand() % 30,
    .waveform = SpeakerWaveformSine,
    .duration_ms = 40,
    .velocity = 30 + rand() % 98,
  };
  speaker_play_tracks(&s_track, 1, s_volume * 2);
}

static void schedule(void);

static void timer_fired(void *ctx) {
  s_timer = NULL;
  play();
  schedule();
}

// Real fires crackle in clusters: mostly long gaps, a third of the time a
// quick follow-up pop.
static void schedule(void) {
  uint32_t ms = (rand() % 3 == 0) ? 60 + rand() % 121 : 400 + rand() % 2201;
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

#endif
