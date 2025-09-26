#ifndef MINYASX_H
#define MINYASX_H

#include <stdbool.h>
#include <stdint.h>

#include "ch32fun.h"
#include "sound/beep_context.h"
#include "sound/play_context.h"

typedef enum {
    FDD_RPM_CONTROL_NONE = 0,
    FDD_RPM_CONTROL_300 = 1,
    FDD_RPM_CONTROL_360 = 2,
    FDD_RPM_CONTROL_9SCDRV = 3,  // 9SCDRVによるOptionSelect同時アサート方式による制御
    FDD_RPM_CONTROL_BPS = 4,     // BPSによる自動判定方式による制御
} fdd_rpm_control_t;

typedef enum {
    FDD_IN_USE_NONE = 0,
    FDD_IN_USE_LED = 1,
} fdd_in_use_mode_t;

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
    bool connected;                 // ドライブが接続されているか
    uint8_t drive_id;               // ドライブID (0-3)
    bool force_ejected;             // メディアの強制排出状態かどうか(論理排出状態)
    bool inserted;                  // メディアが挿入されているか。force_ejectedがtrueのときは必ずfalse
    bool ready;                     // ドライブが準備完了か
    bool mode_select_inverted;      // MODE SELECT信号の極性反転
    fdd_in_use_mode_t in_use_mode;  // IN-USE信号の動作モード
    fdd_rpm_control_t rpm_control;  // 回転数制御方式
    fdd_rpm_mode_t rpm_setting;     // 設定された回転数
    fdd_rpm_mode_t rpm_measured;    // 測定された回転数
    fdd_bps_mode_t bps_measured;    // 測定されたBPS
} drive_status_t;

typedef struct power_status {
    char* label;          // ラベル（例: "VBUS", "+5V", "+12V"）
    uint16_t voltage_mv;  // 電圧 (mV)
    uint16_t current_ma;  // 電流 (mA)
} power_status_t;

typedef struct usbpd_pod {
    uint32_t voltage_mv;  // 電圧 (mV)
    uint32_t current_ma;  // 電流 (mA)
} usbpd_pod_t;

typedef struct usbpd_status {
    bool connected;      // PD接続されているか
    int pdonum;          // PDOの数
    usbpd_pod_t pod[8];  // PDO情報
} usbpd_status_t;

typedef struct minyasx_context {
    // 電源情報
    power_status_t power[3];
    // USB-PD情報
    usbpd_status_t usbpd;
    // ドライブ情報
    drive_status_t drive[2];  // ドライブA/B
    // Playコンテキスト
    play_context_t play;

} minyasx_context_t;

minyasx_context_t* minyasx_init(void);

#endif