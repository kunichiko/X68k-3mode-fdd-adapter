#include "sound/play_control.h"

#include "sound/beep_control.h"

void play_init(minyasx_context_t* ctx) {
    ctx->play.req = false;
    ctx->play.ack = false;
    ctx->play.melody = NULL;
    ctx->play.note_pos = 0;
    // Beepコンテキストの初期化
    beep_init(ctx);
}

void play_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
#if 0
    static uint32_t last_systick_ms = 0;
    static bool last = false;
    if (systick_ms - last_systick_ms < 10) {
        return;
    }
    last_systick_ms = systick_ms;
    if (last) {
        GPIOA->BCR = (1 << 7);  // Buzzer出力をLowに
    } else {
        GPIOA->BSHR = (1 << 7);  // Buzzer出力をHighに
    }
    last = !last;
    return;
#endif

    play_context_t* context = &ctx->play;
    beep_poll(ctx, systick_ms);

    if (context->melody == NULL) {
        return;
    }
    if (!context->req) {
        context->ack = false;
        context->melody = NULL;
        context->beep.req = false;
        context->note_pos = 0;
        return;
    }
    if (context->melody->note_count <= context->note_pos) {
        context->ack = true;
        context->beep.req = false;
        return;
    }
    if (context->beep.req && context->beep.ack) {
        context->beep.req = false;
        context->note_pos++;
        return;
    }
    if (context->beep.ack) {
        // ackが下がるのを待つ
        return;
    }
    // 音を鳴らす
    context->beep.note = context->melody->notes[context->note_pos].note;
    context->beep.length = context->melody->notes[context->note_pos].length;
    context->beep.req = true;
}

void play_start_melody(minyasx_context_t* ctx, play_melody_t* melody) {
    ctx->play.melody = melody;
    ctx->play.note_pos = 0;
    ctx->play.req = true;
    ctx->play.ack = false;
}

play_melody_t melody_boot = {
    .notes = (play_note_t[]){{NOTE_C2, 3}, {NOTE_E2, 3}, {NOTE_G2, 3}, {NOTE_C3, 4}},
    .note_count = 5,
};

play_melody_t melody_power_on = {
    .notes = (play_note_t[]){{NOTE_C2, 1}, {NOTE_E2, 1}, {NOTE_A3, 3}},
    .note_count = 3,
};

play_melody_t melody_power_off = {
    .notes = (play_note_t[]){{NOTE_A3, 1}, {NOTE_E2, 1}, {NOTE_C2, 3}},
    .note_count = 3,
};

play_melody_t melody_key = {
    .notes = (play_note_t[]){{NOTE_C3, 1}},
    .note_count = 1,
};

play_melody_t melody_key_ng = {
    .notes = (play_note_t[]){{NOTE_A2, 1}},
    .note_count = 1,
};

play_melody_t melody_mute_off = {
    .notes = (play_note_t[]){{NOTE_C3, 1}, {NOTE_C3, 1}, {NOTE_C3, 1}},
    .note_count = 3,
};
