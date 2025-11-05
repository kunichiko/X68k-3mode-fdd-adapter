#ifndef X68FDD_CONTROL_H
#define X68FDD_CONTROL_H

#include <stdint.h>

#include "ch32fun.h"
#include "minyasx.h"

void x68fdd_init(minyasx_context_t* ctx);
void x68fdd_poll(minyasx_context_t* ctx, uint32_t systick_ms);

/**
 * preferenceの値に従ってGreenPAKのID値を再設定する
 */
void x68fdd_update_drive_id(minyasx_context_t* ctx);

#endif  // X68FDD_CONTROL_H