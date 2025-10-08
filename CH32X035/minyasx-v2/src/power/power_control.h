#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H
#include "ch32fun.h"
#include "ina3221/ina3221_control.h"

// 電源状態
typedef struct {
    bool pvd_enabled;           // PVDが有効か
    bool low_voltage_detected;  // 低電圧検出フラグ
    bool x68k_power_on;         // X68000の電源状態
    bool fdd_power_on;          // FDD電源状態
} power_state_t;

void power_control_init(minyasx_context_t* ctx);
void power_control_poll(minyasx_context_t* ctx, uint32_t systick_ms);

void enable_fdd_power(minyasx_context_t* ctx, bool enable);
bool fdd_power_is_enabled(void);

// X68000電源ON強制設定（デバッグ用）
void set_force_pwr_on(bool enable);

// キー操作があったことを記録（強制パワーオンタイムアウト用）
void update_key_activity(void);

// PVD（電源電圧検出）の初期化と状態取得
void power_pvd_init(void);
power_state_t* power_get_state(void);

// スリープ状態に遷移（低電圧検出時）
void power_enter_sleep(minyasx_context_t* ctx);

// USB-PDネゴシエーションの再実行（電圧不足時）
bool power_renegotiate_pd(minyasx_context_t* ctx);

#endif  // POWER_CONTROL_H
