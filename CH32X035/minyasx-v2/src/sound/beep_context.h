#ifndef BEEP_CONTEXT_H
#define BEEP_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BEEP_STATE_FLAG_IDLE = 0,     //
    BEEP_STATE_FLAG_PLAYING = 1,  //
    BEEP_STATE_FLAG_STOPPED = 2   //
} _beep_state_t;

typedef struct beep_context {
    bool req;
    bool ack;
    uint8_t note;    // 0 = no note, 1 = C, 2 = D, 3 = E, 4 = F, 5 = G, 6 = A, 7 =
                     // B, upto 16
    uint8_t length;  // 0 = 1/16, 1 = 1/8, 2 = 1/4, 3 = 1/2, 4 = 1, 5 = 2, 6 =
                     // 4, 7 = 8
    // private
    _beep_state_t state;
    _beep_state_t last_state;
    uint32_t start_tick;
    uint32_t last_tick;
    uint8_t flag;
} beep_context_t;

#endif