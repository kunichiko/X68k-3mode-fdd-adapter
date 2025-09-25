#include "minyasx.h"

#include "sound/play_control.h"

minyasx_context_t* minyasx_init(void) {
    static minyasx_context_t ctx;

    // 電源情報の初期化
    ctx.power[0].label = "VBUS";
    ctx.power[1].label = "FD12";
    ctx.power[2].label = "FD05";

    return &ctx;
}