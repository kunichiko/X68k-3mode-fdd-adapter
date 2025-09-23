#include "minyasx.h"

minyasx_context_t* minyasx_init(void) {
    static minyasx_context_t ctx;

    // 電源情報の初期化
    ctx.power[0].label = "VBUS";
    ctx.power[1].label = "FD12";
    ctx.power[2].label = "FD05";
    // ドライブ情報の初期化
    for (int i = 0; i < 2; i++) {
        ctx.drive[i].connected = false;
        ctx.drive[i].media_inserted = false;
        ctx.drive[i].ready = false;
        ctx.drive[i].rpm_setting = FDD_RPM_300;
        ctx.drive[i].rpm_current = FDD_RPM_UNKNOWN;
        ctx.drive[i].bps_setting = BPS_250K;
        ctx.drive[i].bps_current = BPS_UNKNOWN;
    }
    return &ctx;
}