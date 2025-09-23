#ifndef PCFDD_CONTROL_H
#define PCFDD_CONTROL_H

#include <stdint.h>

void pcfdd_init(void);
void pcfdd_poll(uint32_t systick_ms);

/* 別モジュールから現在のDRIVE_SELECT状態を通知する */
typedef enum {
    PCFDD_DS_NONE = 0,
    PCFDD_DS0 = 1,
    PCFDD_DS1 = 2,
} pcfdd_ds_t;

void pcfdd_set_current_ds(pcfdd_ds_t ds);

// bps 判定結果
// 観測しうるBPSカテゴリ
//  - 600k : (500k @300rpm) を 360rpmで読んだ等 → 最短 ~1.67us ≈ 7tick
//  - 500k : 1.2M/1.44M 回転一致 → 最短 2.0us = 8tick
//  - 416k : (500k @360rpm) を 300rpmで読んだ → 最短 ~2.4us ≈ 10tick
//  - 300k : (250k @300rpm) を 360rpmで読んだ → 最短 ~3.33us ≈ 13tick
//  - 250k : 2DD 回転一致 → 最短 4.0us = 16tick
typedef enum {
    BPS_UNKNOWN,
    BPS_250K,
    BPS_300K,
    BPS_416K,
    BPS_500K,
    BPS_600K,
} bps_mode_t;

uint32_t pcfdd_bps_value(bps_mode_t m);

#endif