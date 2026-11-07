#pragma once
#include <pebble.h>

// Crackling fire sound. Compiled only on platforms with a speaker.
#if defined(PBL_SPEAKER)

// Builds the crackle sample. Call once at startup.
void crackle_init(void);

// Starts (or keeps) the crackle timer at volume 1..10; stops if volume is 0.
void crackle_start(uint8_t volume);
void crackle_stop(void);

// Plays one pop now at volume 1..10, so a settings change can be heard.
void crackle_preview(uint8_t volume);

#endif
