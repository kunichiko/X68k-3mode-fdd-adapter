#include "ui/ui_control.h"

void ui_page_main_init(minyasx_context_t* ctx, ui_window_t* win) {
    win->key_callback = ui_page_main_key_callback;
}

void ui_page_main_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    static uint64_t last_tick = 0;
    if (systick_ms - last_tick < 500) {
        return;
    }
    last_tick = systick_ms;

    ui_cursor(UI_PAGE_MAIN, 11 * 6, 0);
    ui_print(UI_PAGE_MAIN, "-Minyas X-");
    // 電源情報の表示
    for (int i = 0; i < 3; i++) {
        ui_cursor(UI_PAGE_MAIN, 11 * 6, 1 + i * 2);
        ui_printf(UI_PAGE_MAIN, "%4s:%2d.%02dV",    //
                  ctx->power[i].label,              //
                  ctx->power[i].voltage_mv / 1000,  //
                  (ctx->power[i].voltage_mv % 1000) / 10);
        ui_cursor(UI_PAGE_MAIN, 11 * 6, 2 + i * 2);
        ui_printf(UI_PAGE_MAIN, "     %4dmA", ctx->power[i].current_ma);
    }
    // ドライブ情報の表示
    for (int i = 0; i < 2; i++) {
        ui_cursor(UI_PAGE_MAIN, 0, 0 + i * 4);
        ui_printf(UI_PAGE_MAIN, "%c[%s]", (i == 0 ? 'A' : 'B'), "ready");
        ui_cursor(UI_PAGE_MAIN, 0, 1 + i * 4);
        ui_printf(UI_PAGE_MAIN, " M:%3drpm", ctx->drive[i].rpm_setting == FDD_RPM_300 ? 300 : 360);
        ui_cursor(UI_PAGE_MAIN, 0, 2 + i * 4);
        ui_printf(UI_PAGE_MAIN, " R:%3drpm", ctx->drive[i].rpm_current == FDD_RPM_300 ? 300 : 360);
        ui_cursor(UI_PAGE_MAIN, 0, 3 + i * 4);
        ui_printf(UI_PAGE_MAIN, " B:%3dbps", fdd_bps_mode_to_value(ctx->drive[i].bps_current) / 1000);
    }
}

void ui_page_main_key_callback(ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メニューページに遷移
        ui_change_page(UI_PAGE_MENU);
    }
}
