#include "sound/beep_control.h"

void beep_init(minyasx_context_t* ctx) {
    // Buzzer端子はPA7 (TIM3_CH2, PWM)
    ctx->play.beep.req = false;
    ctx->play.beep.ack = false;
    ctx->play.beep.note = 0;
    ctx->play.beep.length = 0;
    ctx->play.beep.state = BEEP_STATE_FLAG_IDLE;
    ctx->play.beep.last_state = BEEP_STATE_FLAG_IDLE;
    ctx->play.beep.start_tick = 0;
    ctx->play.beep.last_tick = 0;
    ctx->play.beep.flag = 0;

    // =========================
    // TIM3 CH2: PWM mode 1, preload
    // =========================
    // CC2S=00 (output), OC2M=110 (PWM1), OC2PE=1
    TIM3->CHCTLR1 &= ~(TIM_CC2S | TIM_OC2M);
    TIM3->CHCTLR1 |= (TIM_OC2M_2 | TIM_OC2M_1) | TIM_OC2PE;

    // 出力有効（極性は通常正論理）
    TIM3->CCER &= ~TIM_CC2P;
    TIM3->CCER |= TIM_CC2E;

    // 初期は無音
    TIM3->CH2CVR = 0;
    // ARR は他所で設定済みかもしれないが、ここでは 0 にして停止状態を明示
    TIM3->ATRLR = 0;

    // バッファ反映
    TIM3->SWEVGR |= TIM_UG;

    // 一般タイマなので BDTR/MOE は不要（TIM1 と違う点）
    // カウンタ開始は既に開始済み(=Channel1のINDEX計測で動作中)想定だが、
    // 念のため再度開始する
    TIM3->CTLR1 |= TIM_CEN;
}

static bool mute_ = false;

static int note_freq[] = {0,                                  //
                          131, 147, 165, 175, 196, 220, 247,  //
                          262, 294, 330, 349, 392, 440, 494,  //
                          523, 587, 659, 698, 784, 880, 988, 1047};

void beep_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // Buzzerのポーリングコード
    beep_context_t* context = &ctx->play.beep;
    switch (context->state) {
    case BEEP_STATE_FLAG_IDLE:
        // PWM 無効化（無音）
        TIM3->ATRLR = 0;
        TIM3->CH2CVR = 0;

        if (context->req) {
            context->state = BEEP_STATE_FLAG_PLAYING;
            context->start_tick = systick_ms;
            int hz = note_freq[context->note];
            if (hz > 0 && !mute_) {
                // PSC=1us 刻み想定 → 周期[us] = 1e6 / Hz
                uint32_t period_us = 1000000u / (uint32_t)hz;
                if (period_us == 0) period_us = 1;  // 上限保護

                TIM3->ATRLR = period_us - 1;   // PWM 周期
                TIM3->CH2CVR = period_us / 2;  // 50% duty

                // 一括反映
                TIM3->SWEVGR |= TIM_UG;
            } else {
                TIM3->ATRLR = 0;
                TIM3->CH2CVR = 0;
            }
        }
        break;

    case BEEP_STATE_FLAG_PLAYING:
        if (!context->req) {
            context->state = BEEP_STATE_FLAG_STOPPED;
            break;
        }
        // 16分音符 を 40msec として、長さに応じた時間が経過したら停止
        if (systick_ms - context->start_tick > (40u << context->length)) {
            context->state = BEEP_STATE_FLAG_STOPPED;
            break;
        }
        // 鳴動中は特に処理なし（必要なら音量/デューティ等を可変）
        break;

    case BEEP_STATE_FLAG_STOPPED:
        // PWM 無効化
        TIM3->ATRLR = 0;
        TIM3->CH2CVR = 0;
        context->ack = 1;

        if (!context->req) {
            context->ack = 0;
            context->state = BEEP_STATE_FLAG_IDLE;
        }
        break;
    }
}

void beep_mute(bool mute) {
    mute_ = mute;
}