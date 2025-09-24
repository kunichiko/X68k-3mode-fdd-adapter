#include "ui/ui_control.h"

// Debug page
static void ui_page_debug_poll(ui_page_context_t* ctx, uint32_t systick_ms);
static void ui_page_debug_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);
static void ui_page_debug_keyin_pcfdd(ui_page_context_t* pctx, ui_key_mask_t keys);

void ui_page_debug_init(ui_page_context_t* win) {
    win->enter = NULL;
    win->poll = ui_page_debug_poll;
    win->keyin = ui_page_debug_keyin;
}
void ui_page_debug_init_pcfdd(ui_page_context_t* win) {
    win->enter = NULL;
    win->poll = ui_page_debug_poll;
    win->keyin = ui_page_debug_keyin_pcfdd;
}

void ui_page_debug_poll(ui_page_context_t* ctx, uint32_t systick_ms) {
    if (ui_get_current_page() != UI_PAGE_DEBUG) {
        return;
    }
    // Debugページのポーリング処理
}

void ui_page_debug_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メインページに戻る
        ui_change_page(UI_PAGE_MAIN);
    }
    if (keys & UI_KEY_LEFT) {
        // PCFDDデバッグページに遷移
        ui_change_page(UI_PAGE_DEBUG_PCFDD);
    }
}

void ui_page_debug_keyin_pcfdd(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メインページに戻る
        ui_change_page(UI_PAGE_MAIN);
    }
    if (keys & UI_KEY_RIGHT) {
        // 通常のデバッグページに遷移
        ui_change_page(UI_PAGE_DEBUG);
    }
}
