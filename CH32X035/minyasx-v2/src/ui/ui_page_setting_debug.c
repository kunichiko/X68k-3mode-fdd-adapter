#include "greenpak/greenpak_control.h"
#include "pcfdd/pcfdd_control.h"
#include "power/power_control.h"
#include "ui/ui_control.h"

#define NUM_MENU_ITEMS 5

// debug Setting page
static void ui_page_setting_debug_enter(ui_page_context_t* pctx);
static void ui_page_setting_debug_poll(ui_page_context_t* pctx, uint32_t systick_ms);
static void ui_page_setting_debug_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

void ui_page_setting_debug_init(ui_page_context_t* win) {
    win->enter = ui_page_setting_debug_enter;
    win->poll = ui_page_setting_debug_poll;
    win->keyin = ui_page_setting_debug_keyin;
}
void ui_page_setting_debug_enter(ui_page_context_t* pctx) {
    // debug Settingページの初期化処理
    ui_page_type_t page = pctx->page;
    ui_cursor(page, 0, 0);
    ui_print(page, "[Debug Setting]\n");
    ui_print(page, ">MOTOR [---------]\n");
    ui_print(page, " LOCK  [---------]\n");
    ui_print(page, " FDDPW [---------]\n");
    ui_print(page, " DS A  [---------]\n");
    ui_print(page, " DS B  [---------]\n");
    ui_print(page, "\n");
    ui_print(page, " RETURN");
}

void ui_page_setting_debug_poll(ui_page_context_t* pctx, uint32_t systick_ms) {
    if (ui_get_current_page() != pctx->page) {
        return;
    }
    // debug Settingページのポーリング処理
    // PB4 : MOTOR_ON_DOSV output
    // PBN4の出力状態を読み取り、MOTORのON/OFFを表示する
    // PB7 : LOCK_ACK input
    // PC6 : LOCK input
    uint32_t porta = GPIOA->INDR;
    uint32_t portb = GPIOB->INDR;
    bool motor_enabled = (portb & (1 << 4));  // MOTOR_ON_DOSV is active high
    ui_cursor(pctx->page, 8, 1);
    ui_printf(pctx->page, "%4s", motor_enabled ? "ON       " : "OFF      ");
    // PCFDDのLOCK状態を読み取り、表示する
    uint32_t portc = GPIOC->INDR;
    bool lock_req = (portc & (1 << 6));  // LOCK is active high
    bool lock_ack = (portb & (1 << 7));  // LOCK_ACK is active high
    ui_cursor(pctx->page, 8, 2);
    if (!lock_req && !lock_ack) {
        // Not Request
        ui_printf(pctx->page, "Released ");
    } else if (lock_req && !lock_ack) {
        // Request
        ui_printf(pctx->page, "Request  ");
    } else if (lock_req && lock_ack) {
        // Acknowledge
        ui_printf(pctx->page, "Locked   ");
    } else {
        // Invalid
        ui_printf(pctx->page, "Releasing");
    }
    // FDD Power Enableの状態を読み取り、表示する
    bool fddpw_enabled = fdd_power_is_enabled();
    ui_cursor(pctx->page, 8, 3);
    ui_printf(pctx->page, "%4s", fddpw_enabled ? "ON       " : "OFF      ");
    // DS A (PB2) の状態を読み取り、表示する
    bool ds_a_enabled = (porta & (1 << 0));  // FDD_DS_A is active high
    ui_cursor(pctx->page, 8, 4);
    ui_printf(pctx->page, "%4s", ds_a_enabled ? "ON       " : "OFF      ");
    // DS B (PB3) の状態を読み取り、表示する
    bool ds_b_enabled = (porta & (1 << 1));  // FDD_DS_B is active high
    ui_cursor(pctx->page, 8, 5);
    ui_printf(pctx->page, "%4s", ds_b_enabled ? "ON       " : "OFF      ");
}

//
// Key callback
//
static int position = 1;  // 現在の選択位置 (1-7)

static void set_position(int pos) {
    ui_page_type_t page = UI_PAGE_SETTING_DEBUG;
    ui_cursor(page, 0, position);
    ui_print(page, " ");
    int new_pos = pos;
    if (new_pos < 1) new_pos = 1;
    if (new_pos > 7) new_pos = 7;
    while ((new_pos > NUM_MENU_ITEMS) && (new_pos < 7)) {
        new_pos = (new_pos < position) ? new_pos - 1 : new_pos + 1;  // 空行を飛ばす
    }
    position = new_pos;
    ui_cursor(page, 0, position);
    ui_print(page, ">");
}

void ui_page_setting_debug_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_UP) {
        set_position(position - 1);
    }
    if (keys & UI_KEY_DOWN) {
        set_position(position + 1);
    }
    if (keys & UI_KEY_LEFT) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
        set_position(1);  // 戻しておく
        return;
    }
    if (keys & UI_KEY_ENTER) {
        switch (position) {
        case 1:
            // MOTORのON/OFFをトグルする
            // GreenPAK4の Vitrual Input に以下を接続している
            // 7 (bit0)  = MOTOR_ON (正論理)
            uint8_t gp4_vin = greenpak_get_virtualinput(4 - 1);
            if (gp4_vin & (1 << 0)) {
                // 現在ONなのでOFFにする
                gp4_vin &= ~(1 << 0);  // bit0 = 0 (MOTOR_ON)
            } else {
                // 現在OFFなのでONにする
                gp4_vin |= (1 << 0);  // bit0 = 1 (MOTOR_ON)
            }
            greenpak_set_virtualinput(4 - 1, gp4_vin);
            break;
        case 2:
            // GP ENABLE
            // GP ENABLEのON/OFFをトグルする
            if (GPIOC->INDR & (1 << 6)) {
                // 現在ONなのでOFFにする
                GPIOC->BCR = (1 << 6);  // GP_ENABLE = OFF
            } else {
                // 現在OFFなのでONにする
                GPIOC->BSHR = (1 << 6);  // GP_ENABLE = ON
            }
            break;
        case 3:
            // FDDPW ENA
            // FDD Power EnableのON/OFFをトグルする
            if (fdd_power_is_enabled()) {
                // 現在ONなのでOFFにする
                enable_fdd_power(pctx->ctx, false);
            } else {
                // 現在OFFなのでONにする
                enable_fdd_power(pctx->ctx, true);
            }
            break;
        case 4:
            // DS A
            // ドライブAのDS出力をトグルする
            if (GPIOB->INDR & (1 << 2)) {
                // 現在ONなのでOFFにする
                GPIOB->BCR = (1 << 2);  // FDD_DS_A = OFF
                pcfdd_set_current_ds(PCFDD_DS_NONE);
            } else {
                // 現在OFFなのでONにする
                GPIOB->BCR = (1 << 3);   // FDD_DS_B = OFF
                GPIOB->BSHR = (1 << 2);  // FDD_DS_A = ON
                pcfdd_set_current_ds(PCFDD_DS0);
            }
            break;
        case 5:
            // DS B
            // ドライブBのDS出力をトグルする
            if (GPIOB->INDR & (1 << 3)) {
                // 現在ONなのでOFFにする
                GPIOB->BCR = (1 << 3);  // FDD_DS_B = OFF
                pcfdd_set_current_ds(PCFDD_DS_NONE);
            } else {
                // 現在OFFなのでONにする
                GPIOB->BCR = (1 << 2);   // FDD_DS_A = OFF
                GPIOB->BSHR = (1 << 3);  // FDD_DS_B = ON
                pcfdd_set_current_ds(PCFDD_DS1);
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