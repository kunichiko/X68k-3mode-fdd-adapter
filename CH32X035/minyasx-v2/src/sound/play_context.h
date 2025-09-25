#ifndef PLAY_CONTEXT_H
#define PLAY_CONTEXT_H
#include <stdbool.h>
#include <stdint.h>

#include "sound/beep_context.h"

typedef struct play_note {
    uint8_t note;
    uint8_t length;
} play_note_t;

typedef struct play_melody {
    play_note_t *notes;
    uint8_t note_count;
} play_melody_t;

typedef struct play_context {
    bool req;
    bool ack;
    play_melody_t *melody;
    uint8_t note_pos;
    beep_context_t beep;
} play_context_t;
#endif