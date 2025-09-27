#include "ui_control.h"

// log page
void ui_page_log_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

void ui_page_log_init(ui_page_context_t* pcon) {
    pcon->enter = NULL;
    pcon->poll = NULL;
    pcon->keyin = ui_page_log_keyin;
    pcon->scroll_enable = true;
    pcon->scroll_keepheader = true;
    ui_cursor(pcon->page, 0, 0);
    ui_print(pcon->page, "========[Log]======\n");
}

void ui_page_log_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_RIGHT) {
        // メインページに戻る
        ui_change_page(UI_PAGE_MAIN);
    }
    if (keys & UI_KEY_LEFT) {
        // デバッグページに遷移
        ui_change_page(UI_PAGE_DEBUG);
    }
    if (keys & UI_KEY_ENTER) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
    }
}