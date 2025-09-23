#include "ui/ui_control.h"

void ui_page_pdstatus_init(minyasx_context_t* ctx, ui_window_t* win) {
    win->key_callback = ui_page_pdstatus_key_callback;
    ui_cursor(UI_PAGE_PDSTATUS, 0, 0);
    ui_print(UI_PAGE_PDSTATUS, "[USB-PD Status]\n");
    ui_cursor(UI_PAGE_PDSTATUS, 0, 7);
    ui_print(UI_PAGE_PDSTATUS, ">RETURN");
}

void ui_page_pdstatus_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    if (ui_get_current_page() != UI_PAGE_PDSTATUS) {
        return;
    }
    // USB-PD Statusページのポーリング処理
    if (ctx->usbpd.connected) {
        for (int i = 0; i < 4; i++) {
            if (i >= ctx->usbpd.pdonum) break;
            ui_cursor(UI_PAGE_PDSTATUS, 0, 1 + i);
            ui_printf(UI_PAGE_PDSTATUS, " (%d) %5dmv  %4dmA", i + 1, ctx->usbpd.pod[i].voltage_mv, ctx->usbpd.pod[i].current_ma);
        }
    } else {
        ui_cursor(UI_PAGE_PDSTATUS, 0, 1);
        ui_print(UI_PAGE_PDSTATUS, " Not Connected");
    }
}

void ui_page_pdstatus_key_callback(ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
    }
}