#include "preferences/preferences_control.h"

#include "preferences/ch32_flash.h"
#include "ui/ui_control.h"

_Static_assert(sizeof(preferences_t) < CH32_FLASH_DATA_SIZE, "MyStruct size must be less than 62 bytes");

static void preferences_load(minyasx_context_t* ctx);
static void preferences_save_to_flash(minyasx_context_t* ctx);

void preferences_init(minyasx_context_t* ctx) {
    // ch32flash_dumpflash();
    if (ch32flash_init()) {
        uint8_t* data = (uint8_t*)&ctx->preferences;
        size_t size = sizeof(ctx->preferences);

        for (int i = 0; i < size; i++) {
            // フラッシュメモリから読み出し
            data[i] = ch32flash_get_byte(i);
        }
    } else {
        // フラッシュの初期化に失敗した場合の処理
        ui_logf(UI_LOG_LEVEL_INFO, "Flash CRC Error. Initializing preferences to defaults.\n");
        preferences_load_defaults(ctx);
        preferences_save(ctx);
    }

    // 初期化後、設定を反映する
    preferences_load(ctx);
}

void preferences_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // ここで定期的に設定をチェックし、必要に応じてctx->preferencesを更新する
    // 例えば、UIからの変更を反映するなど
}

#define SIGNATURE "MYSX"
#define PREF_VERSION_M 1
#define PREF_VERSION_S 0

void preferences_load_defaults(minyasx_context_t* ctx) {
    // デフォルト設定をここで定義する
    preferences_t* pref = &ctx->preferences;
    pref->signature[0] = SIGNATURE[0];
    pref->signature[1] = SIGNATURE[1];
    pref->signature[2] = SIGNATURE[2];
    pref->signature[3] = SIGNATURE[3];
    pref->version_m = PREF_VERSION_M;
    pref->version_s = PREF_VERSION_S;
    pref->reserved[0] = 0;
    pref->reserved[1] = 0;
    pref->fdd_rpm_control[0] = FDD_RPM_CONTROL_9SCDRV;  // ドライブAのデフォルトは9SCDRV互換モード
    pref->fdd_rpm_control[1] = FDD_RPM_CONTROL_9SCDRV;  // ドライブBのデフォルトは9SCDRV互換モード
    pref->fdd_in_use_mode[0] = FDD_IN_USE_LED;          // ドライブAのデフォルトはLEDで表示
    pref->fdd_in_use_mode[1] = FDD_IN_USE_LED;          // ドライブBのデフォルトはLEDで表示
    pref->mode_select_inverted[0] = false;              // ドライブAのMODE SELECT信号の極性反転なし
    pref->mode_select_inverted[1] = false;              // ドライブBのMODE SELECT信号の極性反転なし
    pref->media_auto_detect = MEDIA_AUTO_DETECT_60SEC;  // デフォルトは60秒間トライ
    pref->speaker_enabled = true;                       // デフォルトはスピーカー有効
}

void preferences_apply(minyasx_context_t* ctx) {
    // ctx->preferencesの設定をctx->drive[]に適用
    ctx->drive[0].rpm_control = ctx->preferences.fdd_rpm_control[0];
    ctx->drive[1].rpm_control = ctx->preferences.fdd_rpm_control[1];
    ctx->drive[0].in_use_mode = ctx->preferences.fdd_in_use_mode[0];
    ctx->drive[1].in_use_mode = ctx->preferences.fdd_in_use_mode[1];
    ctx->drive[0].mode_select_inverted = ctx->preferences.mode_select_inverted[0];
    ctx->drive[1].mode_select_inverted = ctx->preferences.mode_select_inverted[1];
}

static void preferences_load(minyasx_context_t* ctx) {
    preferences_t* pref = &ctx->preferences;
    if (pref->signature[0] != SIGNATURE[0] || pref->signature[1] != SIGNATURE[1] ||  //
        pref->signature[2] != SIGNATURE[2] || pref->signature[3] != SIGNATURE[3] ||  //
        pref->version_m != PREF_VERSION_M) {
        // シグネチャまたはメジャーバージョンが異なる場合はデフォルト設定をロード
        ui_logf(UI_LOG_LEVEL_INFO, "Preferences signature/version mismatch. Loading defaults.");
        ch32flash_clear_data();
        preferences_load_defaults(ctx);
        preferences_save(ctx);
        return;
    }
    preferences_apply(ctx);
}

void preferences_save(minyasx_context_t* ctx) {
    ctx->preferences.signature[0] = SIGNATURE[0];
    ctx->preferences.signature[1] = SIGNATURE[1];
    ctx->preferences.signature[2] = SIGNATURE[2];
    ctx->preferences.signature[3] = SIGNATURE[3];
    ctx->preferences.version_m = PREF_VERSION_M;
    ctx->preferences.version_s = PREF_VERSION_S;
    ctx->preferences.reserved[0] = 0;
    ctx->preferences.reserved[1] = 0;
    ctx->preferences.fdd_rpm_control[0] = ctx->drive[0].rpm_control;
    ctx->preferences.fdd_rpm_control[1] = ctx->drive[1].rpm_control;
    ctx->preferences.fdd_in_use_mode[0] = ctx->drive[0].in_use_mode;
    ctx->preferences.fdd_in_use_mode[1] = ctx->drive[1].in_use_mode;
    ctx->preferences.mode_select_inverted[0] = ctx->drive[0].mode_select_inverted;
    ctx->preferences.mode_select_inverted[1] = ctx->drive[1].mode_select_inverted;
    // ctx->preferencesの内容をフラッシュメモリに保存する
    preferences_save_to_flash(ctx);
}

static void preferences_save_to_flash(minyasx_context_t* ctx) {
    // ここでctx->preferencesの内容をフラッシュメモリなどに保存する
    uint8_t* data = (uint8_t*)&ctx->preferences;
    size_t size = sizeof(ctx->preferences);

    ui_logf(UI_LOG_LEVEL_INFO, "Flash Write Size=%d\n", (int)size);
    for (int i = 0; i < size; i++) {
        // フラッシュメモリへの書き込み処理
        //        ui_logf(UI_LOG_LEVEL_INFO, "%02d ", data[i]);
        ch32flash_set_byte(i, data[i]);
    }
    //    ui_logf(UI_LOG_LEVEL_INFO, "\n");
    ch32flash_commit();  // 書き込みを確定
}