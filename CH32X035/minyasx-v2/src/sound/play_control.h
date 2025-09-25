#ifndef PLAY_CONTROL_H
#define PLAY_CONTROL_H

#include "minyasx.h"

void play_init(minyasx_context_t* ctx);
void play_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void play_mute(bool mute);
void play_start_melody(minyasx_context_t* ctx, play_melody_t* melody);

extern play_melody_t melody_boot;
extern play_melody_t melody_power_on;
extern play_melody_t melody_power_off;
extern play_melody_t melody_key;
extern play_melody_t melody_key_ng;
extern play_melody_t melody_mute_off;

#endif