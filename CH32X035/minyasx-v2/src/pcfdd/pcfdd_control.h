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

void pcfdd_set_rpm_mode_select(drive_status_t* drive, fdd_rpm_mode_t rpm);

uint32_t fdd_bps_mode_to_value(fdd_bps_mode_t m);

/**
 * 設定が変更されたら呼び出す関数
 */
void pcfdd_update_setting(minyasx_context_t* ctx, int drive);

/**
 * イジェクト操作を試みます
 * EJECT_MASKが有効な場合は無視されます
 */
void pcfdd_try_eject(minyasx_context_t* ctx, int drive);

/**
 * ドライブを強制的にイジェクト状態にします
 */
void pcfdd_force_eject(minyasx_context_t* ctx, int drive);

/**
 * ドライブのメディア挿入状態を検出し、挿入状態を更新します。
 * force_eject状態も解除されます。
 */
void pcfdd_detect_media(minyasx_context_t* ctx, int drive);  // drive: 0=FDD_A or 1=FDD_B

char* pcfdd_state_to_string(drive_state_t state);

#endif