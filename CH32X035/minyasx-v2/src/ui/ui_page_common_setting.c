#include "ui/ui_control.h"

// Common Setting page
static void ui_page_setting_common_enter(ui_page_context_t* pctx);
static void ui_page_setting_common_poll(ui_page_context_t* pctx, uint32_t systick_ms);
static void ui_page_setting_common_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

void ui_page_setting_common_init(ui_page_context_t* win) {
    win->enter = ui_page_setting_common_enter;
    win->poll = ui_page_setting_common_poll;
    win->keyin = ui_page_setting_common_keyin;
}
void ui_page_setting_common_enter(ui_page_context_t* pctx) {
    minyasx_context_t* ctx = pctx->ctx;
    // Common Settingページの初期化処理
    ui_page_type_t page = pctx->page;
    ui_cursor(page, 0, 0);
    ui_print(page, "[Common Setting]\n");
    ui_printf(page, ">MOTOR   [----]\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, " RETURN");
}

void ui_page_setting_common_poll(ui_page_context_t* pctx, uint32_t systick_ms) {
    if (ui_get_current_page() != pctx->page) {
        return;
    }
    // Common Settingページのポーリング処理
    // PB4 : MOTOR_ON_DOSV output
    // PBN4の出力状態を読み取り、MOTORのON/OFFを表示する
    minyasx_context_t* ctx = pctx->ctx;
    uint32_t portb = GPIOB->INDR;
    bool motor_enabled = (portb & (1 << 4));  // MOTOR_ON_DOSV is active high
    ui_cursor(pctx->page, 10, 1);
    ui_printf(pctx->page, "%4s", motor_enabled ? "ON  " : "OFF ");
}

//
// Key callback
//
static int position = 1;  // 現在の選択位置 (1-7)

static void set_position(int pos) {
    ui_page_type_t page = UI_PAGE_SETTING_COMMON;
    ui_cursor(page, 0, position);
    ui_print(page, " ");
    int new_pos = pos;
    if (new_pos < 1) new_pos = 1;
    if (new_pos > 7) new_pos = 7;
    while ((new_pos >= 2) && (new_pos <= 6)) {
        new_pos = (new_pos < position) ? new_pos - 1 : new_pos + 1;  // 空行を飛ばす
    }
    position = new_pos;
    ui_cursor(page, 0, position);
    ui_print(page, ">");
}

void ui_page_setting_common_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_UP) {
        set_position(position - 1);
    }
    if (keys & UI_KEY_DOWN) {
        set_position(position + 1);
    }
    if (keys & UI_KEY_ENTER) {
        switch (position) {
        case 1:
            // MOTOR
            // MOTORのON/OFFをトグルする
            if (GPIOB->INDR & (1 << 4)) {
                // 現在ONなのでOFFにする
                GPIOB->BCR = (1 << 4);  // MOTOR_ON_DOSV = OFF
            } else {
                // 現在OFFなのでONにする
                GPIOB->BSHR = (1 << 4);  // MOTOR_ON_DOSV = ON
            }
            break;
        case 7:
            // RETURN
            // メニューページに戻る
            ui_change_page(UI_PAGE_MENU);
            set_position(1);  // 戻しておく
            break;
        default:
            break;
        }
    }
}