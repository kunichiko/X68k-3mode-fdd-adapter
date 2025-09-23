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

// bps
typedef enum {
    BPS_UNKNOWN,
    BPS_250K,
    BPS_300K,
    BPS_416K,
    BPS_500K,
    BPS_600K,
} fdd_bps_mode_t;

typedef struct drive_context {
    bool connected;              // ドライブが接続されているか
    bool media_inserted;         // メディアが挿入されているか
    bool ready;                  // ドライブが準備完了か
    fdd_rpm_mode_t rpm_setting;  // 設定された回転数
    fdd_rpm_mode_t current_rpm;  // 現在の回転数
    fdd_bps_mode_t bps_setting;  // 設定されたBPS
    fdd_bps_mode_t current_bps;  // 現在のBPS
} drive_context_t;

typedef struct minyasx_context {
    drive_context_t drive[2];  // ドライブA/B
} minyasx_context_t;

minyasx_context_t* minyasx_init(void);

#endif