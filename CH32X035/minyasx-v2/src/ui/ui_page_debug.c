#include "ui/ui_control.h"

void ui_page_debug_init(minyasx_context_t* ctx, ui_window_t* win) {
    win->key_callback = ui_page_debug_key_callback;
}

void ui_page_debug_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    if (ui_get_current_page() != UI_PAGE_DEBUG) {
        return;
    }
    // Debugページのポーリング処理
}

void ui_page_debug_key_callback(ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // メインページに戻る
        ui_change_page(UI_PAGE_MAIN);
    }
}