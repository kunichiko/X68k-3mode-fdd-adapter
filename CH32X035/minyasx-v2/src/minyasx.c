#include "minyasx.h"

minyasx_context_t* minyasx_init(void) {
    static minyasx_context_t ctx;
    for (int i = 0; i < 2; i++) {
        ctx.drive[i].connected = false;
        ctx.drive[i].media_inserted = false;
        ctx.drive[i].ready = false;
        ctx.drive[i].rpm_setting = FDD_RPM_300;
        ctx.drive[i].current_rpm = FDD_RPM_UNKNOWN;
        ctx.drive[i].bps_setting = BPS_250K;
        ctx.drive[i].current_bps = BPS_UNKNOWN;
    }
    return &ctx;
}