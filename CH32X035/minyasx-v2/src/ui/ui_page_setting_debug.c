#include "greenpak/greenpak_control.h"
#include "led/led_control.h"
#include "pcfdd/pcfdd_control.h"
#include "power/power_control.h"
#include "ui/ui_control.h"

#define NUM_MENU_ITEMS 6

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
    ui_print(page, ">MOTOR    [--------]\n");
    ui_print(page, " LOCK     [--------]\n");
    ui_print(page, " FDD Pwr  [--------]\n");
    ui_print(page, " LED Test [--------]\n");
    ui_print(page, " DS A     [--------]\n");
    ui_print(page, " DS B     [--------]\n");
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
    ui_cursor(pctx->page, 11, 1);
    ui_print(pctx->page, motor_enabled ? "ON      " : "OFF     ");
    // PCFDDのLOCK状態を読み取り、表示する
    uint32_t portc = GPIOC->INDR;
    bool lock_req = (portc & (1 << 6));  // LOCK is active high
    bool lock_ack = (portb & (1 << 7));  // LOCK_ACK is active high
    ui_cursor(pctx->page, 11, 2);
    if (!lock_req && !lock_ack) {
        // Not Request
        ui_print(pctx->page, "Released");
    } else if (lock_req && !lock_ack) {
        // Request
        ui_print(pctx->page, "Request ");
    } else if (lock_req && lock_ack) {
        // Acknowledge
        ui_print(pctx->page, "Locked  ");
    } else {
        // Invalid
        ui_print(pctx->page, "Relasing");
    }
    // FDD Power Enableの状態を読み取り、表示する
    bool fddpw_enabled = fdd_power_is_enabled();
    ui_cursor(pctx->page, 11, 3);
    ui_print(pctx->page, fddpw_enabled ? "ON      " : "OFF     ");
    // LED Testの状態を読み取り、表示する
    led_test_mode_t led_mode = WS2812_SPI_get_test_mode();
    ui_cursor(pctx->page, 11, 4);
    switch (led_mode) {
    case LED_TEST_MODE_NORMAL:
        ui_print(pctx->page, "Normal  ");
        break;
    case LED_TEST_MODE_ALL_ON:
        ui_print(pctx->page, "All ON  ");
        break;
    case LED_TEST_MODE_ALL_OFF:
        ui_print(pctx->page, "All OFF ");
        break;
    case LED_TEST_MODE_BLINK:
        ui_print(pctx->page, "Blink   ");
        break;
    }
    // DS A (PB2) の状態を読み取り、表示する
    bool ds_a_enabled = (porta & (1 << 0));  // FDD_DS_A is active high
    ui_cursor(pctx->page, 11, 5);
    ui_print(pctx->page, ds_a_enabled ? "ON      " : "OFF     ");
    // DS B (PB3) の状態を読み取り、表示する
    bool ds_b_enabled = (porta & (1 << 1));  // FDD_DS_B is active high
    ui_cursor(pctx->page, 11, 6);
    ui_print(pctx->page, ds_b_enabled ? "ON      " : "OFF     ");
}

//
// Key callback
//
static int position = 1;  // 現在の選択位置 (1-8)

static void set_position(int pos) {
    ui_page_type_t page = UI_PAGE_SETTING_DEBUG;
    ui_cursor(page, 0, position);
    ui_print(page, " ");
    int new_pos = pos;
    if (new_pos < 1) new_pos = 1;
    if (new_pos > 8) new_pos = 8;
    while ((new_pos > NUM_MENU_ITEMS) && (new_pos < 8)) {
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
            // LED Test - トグル動作（Normal→All ON→All OFF→Normal...）
            {
                led_test_mode_t current_mode = WS2812_SPI_get_test_mode();
                led_test_mode_t next_mode;
                switch (current_mode) {
                case LED_TEST_MODE_NORMAL:
                    next_mode = LED_TEST_MODE_ALL_ON;
                    break;
                case LED_TEST_MODE_ALL_ON:
                    next_mode = LED_TEST_MODE_ALL_OFF;
                    break;
                case LED_TEST_MODE_ALL_OFF:
                    next_mode = LED_TEST_MODE_BLINK;
                    break;
                case LED_TEST_MODE_BLINK:
                    next_mode = LED_TEST_MODE_NORMAL;
                    break;
                default:
                    next_mode = LED_TEST_MODE_NORMAL;
                    break;
                }
                WS2812_SPI_set_test_mode(next_mode);
            }
            break;
        case 5:
            // DS A
            // ドライブAのDS出力をトグルする
            if (GPIOA->INDR & (1 << 0)) {
                // 現在ONなのでOFFにする
                pcfdd_drive_select(0, false);
                pcfdd_set_current_ds(PCFDD_DS_NONE);
            } else {
                // 現在OFFなのでONにする
                pcfdd_drive_select(0, true);
                pcfdd_set_current_ds(PCFDD_DS0);
            }
            break;
        case 6:
            // DS B
            // ドライブBのDS出力をトグルする
            if (GPIOA->INDR & (1 << 1)) {
                // 現在ONなのでOFFにする
                pcfdd_drive_select(1, false);
                pcfdd_set_current_ds(PCFDD_DS_NONE);
            } else {
                // 現在OFFなのでONにする
                pcfdd_drive_select(1, true);
                pcfdd_set_current_ds(PCFDD_DS1);
            }
            break;
        case 8:
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