#include "ui/ui_control.h"

// USB-PD Status page
void ui_page_pdstatus_enter(ui_page_context_t* pctx);
void ui_page_pdstatus_poll(ui_page_context_t* pctx, uint32_t systick_ms);
void ui_page_pdstatus_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

void ui_page_pdstatus_init(ui_page_context_t* win) {
    win->enter = NULL;
    win->poll = ui_page_pdstatus_poll;
    win->keyin = ui_page_pdstatus_keyin;

    ui_page_type_t page = win->page;
    ui_cursor(page, 0, 0);
    ui_print(page, "[USB-PD Status]\n");
    ui_cursor(page, 0, 7);
    ui_print(page, ">RETURN");
}

void ui_page_pdstatus_poll(ui_page_context_t* pctx, uint32_t systick_ms) {
    ui_page_type_t page = pctx->page;
    minyasx_context_t* ctx = pctx->ctx;
    if (ui_get_current_page() != page) {
        return;
    }
    // USB-PD Statusページのポーリング処理
    if (ctx->usbpd.connected) {
        for (int i = 0; i < 4; i++) {
            if (i >= ctx->usbpd.pdonum) break;
            ui_cursor(page, 0, 1 + i);
            ui_printf(page, " (%d) %5dmv  %4dmA", i + 1, ctx->usbpd.pod[i].voltage_mv, ctx->usbpd.pod[i].current_ma);
        }
    } else {
        ui_cursor(page, 0, 1);
        ui_print(page, " Not Connected");
    }
}

void ui_page_pdstatus_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
    }
}