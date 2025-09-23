#include "ui/ui_control.h"

static int position = 1;  // メニューの選択行

void ui_page_menu_init(minyasx_context_t* ctx, ui_window_t* win) {
    win->key_callback = ui_page_menu_key_callback;
    // メニューページの初期化処理
    ui_cursor(UI_PAGE_MENU, 0, 0);
    ui_print(UI_PAGE_MENU, "[Menu]\n");
    ui_print(UI_PAGE_MENU, ">About\n");
    ui_print(UI_PAGE_MENU, " USB-PD Status\n");
    ui_print(UI_PAGE_MENU, " Common Setting\n");
    ui_print(UI_PAGE_MENU, " FDD A Setting\n");
    ui_print(UI_PAGE_MENU, " FDD B Setting\n");
    ui_cursor(UI_PAGE_MENU, 0, 7);
    ui_print(UI_PAGE_MENU, " RETURN");
}

void ui_page_menu_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // メニューのポーリング処理
}

void set_position(int pos) {
    ui_cursor(UI_PAGE_MENU, 0, position);
    ui_print(UI_PAGE_MENU, " ");
    int new_pos = pos;
    if (new_pos < 1) new_pos = 1;
    if (new_pos > 7) new_pos = 7;
    if (new_pos == 6) new_pos = (pos < position) ? 5 : 7;  // 空行を飛ばす
    position = new_pos;
    ui_cursor(UI_PAGE_MENU, 0, position);
    ui_print(UI_PAGE_MENU, ">");
}
void ui_page_menu_key_callback(ui_key_mask_t keys) {
    if (keys & UI_KEY_UP) {
        set_position(position - 1);
    } else if (keys & UI_KEY_DOWN) {
        set_position(position + 1);
    }
    ui_cursor(UI_PAGE_MENU, 0, position);
    ui_print(UI_PAGE_MENU, ">");

    // Enterキーで選択されたメニューを実行
    if (keys & UI_KEY_ENTER) {
        switch (position) {
        case 1:
            // About
            ui_change_page(UI_PAGE_ABOUT);
            break;
        case 2:
            // USB-PD Status
            ui_change_page(UI_PAGE_PDSTATUS);
            break;
        case 3:
            // Common Setting TODO
            break;
        case 4:
            // FDD A Setting TODO
            break;
        case 5:
            // FDD B Setting TODO
            break;
        case 7:
            // RETURN
            set_position(1);  // 戻しておく
            ui_change_page(UI_PAGE_MAIN);
            break;
        default:
            break;
        }
    }
}
