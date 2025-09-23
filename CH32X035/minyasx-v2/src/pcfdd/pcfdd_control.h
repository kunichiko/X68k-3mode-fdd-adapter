#ifndef PCFDD_CONTROL_H
#define PCFDD_CONTROL_H

#include <stdint.h>

#include "minyasx.h"

void pcfdd_init(void);
void pcfdd_poll(minyasx_context_t* ctx, uint32_t systick_ms);

/* 別モジュールから現在のDRIVE_SELECT状態を通知する */
typedef enum {
    PCFDD_DS_NONE = 0,
    PCFDD_DS0 = 1,
    PCFDD_DS1 = 2,
} pcfdd_ds_t;

void pcfdd_set_current_ds(pcfdd_ds_t ds);

uint32_t fdd_bps_mode_to_value(fdd_bps_mode_t m);

#endif