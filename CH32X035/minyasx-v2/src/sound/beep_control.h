#ifndef BEEP_CONTROL_H
#define BEEP_CONTROL_H

#include "minyasx.h"

void beep_init(minyasx_context_t* ctx);
void beep_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void beep_mute(bool mute);

#define NOTE_C1 1
#define NOTE_D1 2
#define NOTE_E1 3
#define NOTE_F1 4
#define NOTE_G1 5
#define NOTE_A2 6
#define NOTE_B2 7
#define NOTE_C2 8
#define NOTE_D2 9
#define NOTE_E2 10
#define NOTE_F2 11
#define NOTE_G2 12
#define NOTE_A3 13
#define NOTE_B3 14
#define NOTE_C3 15
#define NOTE_D3 16

#endif