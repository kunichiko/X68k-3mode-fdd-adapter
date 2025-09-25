#ifndef PCFDD_CONTROL_H
#define PCFDD_CONTROL_H

#include <stdint.h>

#include "minyasx.h"

void pcfdd_init(minyasx_context_t* ctx);
void pcfdd_poll(minyasx_context_t* ctx, uint32_t systick_ms);

/* 別モジュールから現在のDRIVE_SELECT状態を通知する */
typedef enum {
    PCFDD_DS_NONE = 0,
    PCFDD_DS0 = 1,
    PCFDD_DS1 = 2,
} pcfdd_ds_t;

void pcfdd_set_current_ds(pcfdd_ds_t ds);

void set_mode_select(drive_status_t* drive, fdd_rpm_mode_t rpm);

uint32_t fdd_bps_mode_to_value(fdd_bps_mode_t m);

/**
 * 設定が変更されたら呼び出す関数
 */
void pcfdd_update_setting(minyasx_context_t* ctx, int drive);

/**
 * ドライブのメディア挿入状態を設定する
 * media_inserted=trueで挿入、falseでイジェクト
 */
void pcfdd_set_media_inserted(minyasx_context_t* ctx, int drive, bool media_inserted);  // drive: 0=FDD_A or 1=FDD_B

#endif