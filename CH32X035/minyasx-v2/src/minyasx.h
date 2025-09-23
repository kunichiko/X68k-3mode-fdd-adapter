#ifndef MINYASX_H
#define MINYASX_H

#include <stdbool.h>
#include <stdint.h>

#include "ch32fun.h"

typedef enum {
    FDD_RPM_UNKNOWN,
    FDD_RPM_300,
    FDD_RPM_360,
} fdd_rpm_mode_t;

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
} fdd_bps_mode_t;

typedef struct drive_status {
    bool connected;              // ドライブが接続されているか
    uint8_t drive_id;            // ドライブID (0-3)
    bool media_inserted;         // メディアが挿入されているか
    bool ready;                  // ドライブが準備完了か
    fdd_rpm_mode_t rpm_setting;  // 設定された回転数
    fdd_rpm_mode_t rpm_current;  // 現在の回転数
    fdd_bps_mode_t bps_setting;  // 設定されたBPS
    fdd_bps_mode_t bps_current;  // 現在のBPS
} drive_status_t;

typedef struct power_status {
    char* label;          // ラベル（例: "VBUS", "+5V", "+12V"）
    uint16_t voltage_mv;  // 電圧 (mV)
    uint16_t current_ma;  // 電流 (mA)
} power_status_t;

typedef struct minyasx_context {
    // 電源情報
    power_status_t power[3];
    // ドライブ情報
    drive_status_t drive[2];  // ドライブA/B
} minyasx_context_t;

minyasx_context_t* minyasx_init(void);

#endif