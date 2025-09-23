#include "ui_control.h"

void ui_page_about_init(minyasx_context_t* ctx, ui_window_t* win) {
    win->key_callback = ui_page_about_key_callback;

    ui_cursor(UI_PAGE_ABOUT, 0, 0);
    ui_print(UI_PAGE_ABOUT, "[About]\n");
    ui_print(UI_PAGE_ABOUT, " Minyas-X by @kunichiko\n");
    ui_print(UI_PAGE_ABOUT, " PCB Version 2.0.2\n");
    ui_print(UI_PAGE_ABOUT, " Firm Version 2.0.0\n");
    ui_print(UI_PAGE_ABOUT, " FDD A:\n");
    ui_print(UI_PAGE_ABOUT, " FDD B:\n");
    ui_cursor(UI_PAGE_ABOUT, 0, 7);
    ui_print(UI_PAGE_ABOUT, ">RETURN");
}

void ui_page_about_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // Aboutページのポーリング処理
    for (int i = 0; i < 2; i++) {
        ui_cursor(UI_PAGE_ABOUT, 7 * 6, 4 + i);
        if (ctx->drive[0].connected) {
            ui_printf(UI_PAGE_ABOUT, " ID%d", ctx->drive[0].drive_id);
        } else {
            ui_print(UI_PAGE_ABOUT, " N/A");
        }
    }
}

void ui_page_about_key_callback(ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
    }
}