#include "pcfdd/pcfdd_control.h"
#include "ui/ui_control.h"

// main page
static void ui_page_main_enter(ui_page_context_t* pctx);
void ui_page_main_poll(ui_page_context_t* pctx, uint32_t systick_ms);
void ui_page_main_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

// Enterキーが押されたまま画面に入った場合のフラグ
static bool enter_key_held_on_enter = false;

void ui_page_main_init(ui_page_context_t* pctx) {
    pctx->enter = ui_page_main_enter;
    pctx->poll = ui_page_main_poll;
    pctx->keyin = ui_page_main_keyin;
    ui_clear(UI_PAGE_MAIN);
    ui_cursor(UI_PAGE_MAIN, 10, 0);
    ui_print(UI_PAGE_MAIN, "-Minyas X-");
}

void ui_page_main_enter(ui_page_context_t* pctx) {
    // 画面に入る時点でEnterキーが押されているかチェック
    if (ui_is_key_pressed(UI_KEY_ENTER)) {
        // Enterキーが押されたまま画面に入った
        enter_key_held_on_enter = true;
    } else {
        enter_key_held_on_enter = false;
    }

    // MI68-2025のロゴを表示
    ui_cursor(UI_PAGE_MAIN, 10, 7);
    char buffer[12] = "=MI68-2025=";
    for (int i = 0; i < 12 - 1; i++) {
        buffer[i] |= 0x80;  // 反転表示
    }
    ui_print(UI_PAGE_MAIN, buffer);
}

void ui_page_main_poll(ui_page_context_t* pctx, uint32_t systick_ms) {
    static uint64_t last_tick = 0;
    ui_page_type_t page = pctx->page;
    minyasx_context_t* ctx = pctx->ctx;

    // Enterキーが離されたかチェック
    if (enter_key_held_on_enter) {
        if (!ui_is_key_pressed(UI_KEY_ENTER)) {
            // Enterキーが離された
            enter_key_held_on_enter = false;
        }
    }

    if (systick_ms - last_tick < 500) {
        return;
    }
    last_tick = systick_ms;

    // 電源情報の表示
    for (int i = 0; i < 3; i++) {
        ui_cursor(page, 10, 1 + i * 2);
        ui_printf(page, "%4s:%2d.%02dV",            //
                  ctx->power[i].label,              //
                  ctx->power[i].voltage_mv / 1000,  //
                  (ctx->power[i].voltage_mv % 1000) / 10);
        ui_cursor(page, 10, 2 + i * 2);
        ui_printf(page, "     %4dmA", ctx->power[i].current_ma);
    }
    // ドライブ情報の表示
    for (int i = 0; i < 2; i++) {
        ui_cursor(page, 0, 0 + i * 4);
        // MEDIA_WAITING状態の場合は1秒毎に点滅表示
        if (ctx->drive[i].state == DRIVE_STATE_MEDIA_WAITING) {
            if ((systick_ms / 1000) % 2 == 0) {
                ui_printf(page, "%c[%s]", (i == 0 ? 'A' : 'B'), pcfdd_state_to_string(ctx->drive[i].state));
            } else {
                ui_printf(page, "%c[      ]", (i == 0 ? 'A' : 'B'));
            }
        } else {
            ui_printf(page, "%c[%s]", (i == 0 ? 'A' : 'B'), pcfdd_state_to_string(ctx->drive[i].state));
        }
        ui_cursor(page, 0, 1 + i * 4);
        ui_printf(page, " S:%3drpm", ctx->drive[i].rpm_setting == FDD_RPM_300 ? 300 : 360);
        //
        ui_cursor(page, 0, 2 + i * 4);
        if (ctx->drive[i].rpm_measured == FDD_RPM_UNKNOWN) {
            ui_print(page, " M:---rpm");
        } else {
            ui_printf(page, " M:%3drpm", ctx->drive[i].rpm_measured == FDD_RPM_300 ? 300 : 360);
        }
        //
        ui_cursor(page, 0, 3 + i * 4);
        if (ctx->drive[i].bps_measured == BPS_UNKNOWN) {
            ui_print(page, " M:---k");
        } else {
            int bps = fdd_bps_mode_to_value(ctx->drive[i].bps_measured) / 1000;
            ui_printf(page, " M:%3dk", bps);
        }
    }
}

void ui_page_main_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    // Enterキーが押されたまま画面に入った場合は、離されるまでキー入力を無視
    if (enter_key_held_on_enter) {
        return;
    }

    if (keys & UI_KEY_ENTER) {
        // メニューページに遷移
        ui_change_page(UI_PAGE_MENU);
    }
    if (keys & UI_KEY_LEFT) {
        // ログページに遷移
        ui_change_page(UI_PAGE_LOG);
    }
    if (keys & UI_KEY_EJECT_A) {
        // ドライブAのイジェクトボタン
        if (pctx->ctx->drive[0].state == DRIVE_STATE_READY) {
            // 既に挿入されている場合は排出を試みる
            pcfdd_try_eject(pctx->ctx, 0);
            return;
        } else if (pctx->ctx->drive[0].state == DRIVE_STATE_MEDIA_WAITING) {
            // 挿入されていない場合は挿入を試みる
            pcfdd_detect_media(pctx->ctx, 0);
            return;
        } else if (pctx->ctx->drive[0].state == DRIVE_STATE_EJECTED) {
            // EJECTED状態の場合はメディア検出を試みる
            pcfdd_detect_media(pctx->ctx, 0);
            return;
        }
    }
    if (keys & UI_KEY_EJECT_B) {
        // ドライブBのイジェクトボタン
        if (pctx->ctx->drive[1].state == DRIVE_STATE_READY) {
            // 既に挿入されている場合は排出を試みる
            pcfdd_try_eject(pctx->ctx, 1);
            return;
        } else if (pctx->ctx->drive[1].state == DRIVE_STATE_MEDIA_WAITING) {
            // 挿入されていない場合は挿入を試みる
            pcfdd_detect_media(pctx->ctx, 1);
            return;
        } else if (pctx->ctx->drive[1].state == DRIVE_STATE_EJECTED) {
            // EJECTED状態の場合はメディア検出を試みる
            pcfdd_detect_media(pctx->ctx, 1);
            return;
        }
    }
}
