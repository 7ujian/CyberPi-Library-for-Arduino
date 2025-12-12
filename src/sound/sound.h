#ifndef __SOUND_H__
#define __SOUND_H__

#include <Arduino.h>
#include <stdint.h>

// Functions exposed to the main library
void sound_init(void);
void sound_play_note(uint8_t channel, uint8_t pitch, uint8_t duration);
void sound_set_instrument(uint8_t instrument);
void sound_set_volume(uint8_t channel, uint8_t volume);
void sound_stop(void);

#endif