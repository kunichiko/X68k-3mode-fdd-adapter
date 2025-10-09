#include "oled/ssd1306_txt.h"
#include "pcfdd/pcfdd_control.h"
#include "power/power_control.h"
#include "ui_control.h"

// boot page
static void ui_page_boot_enter(ui_page_context_t* pctx);
void ui_page_boot_poll(ui_page_context_t* pctx, uint32_t systick_ms);
void ui_page_boot_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

// スリープ状態管理
static uint32_t sleep_start_ms = 0;
static bool oled_cleared = false;

void ui_page_boot_init(ui_page_context_t* win) {
    win->enter = ui_page_boot_enter;
    win->poll = ui_page_boot_poll;
    win->keyin = ui_page_boot_keyin;

    ui_page_type_t page = win->page;

    ui_cursor(page, 0, 2);
    ui_print(page, "      Minyas X\n");
}

void ui_page_boot_enter(ui_page_context_t* pctx) {
}

void ui_page_boot_poll(ui_page_context_t* pctx, uint32_t systick_ms) {
    static bool last_power_on = true;

    minyasx_context_t* ctx = pctx->ctx;

    // 電源状態の変化を検出（ページに関係なく監視）
    if (last_power_on && !ctx->power_on) {
        // 電源がOFFになった（スリープ開始）
        sleep_start_ms = systick_ms;
        oled_cleared = false;
    } else if (!last_power_on && ctx->power_on) {
        // 電源がONになった（スリープ復帰）
        if (oled_cleared) {
            // OLEDを再表示
            OLED_display(1);
            oled_cleared = false;
        }
    }
    last_power_on = ctx->power_on;

    if (ui_get_current_page() != UI_PAGE_BOOT) {
        return;
    }
    ui_page_type_t page = UI_PAGE_BOOT;

    // 表示のみを行う（画面遷移はmainループで管理）
    if (!ctx->power_on) {
        ui_cursor(page, 0, 3);
        ui_print(page, "    - Sleeping -\n");

        // スリープ開始から60秒経過したらOLEDをクリア
        if (!oled_cleared && (systick_ms - sleep_start_ms >= 60000)) {
            OLED_display(0);
            oled_cleared = true;
        }
    }
}

static uint32_t key_enter_press_start_ms = 0;
static bool key_enter_first_press = false;
static bool force_pwr_activated = false;

void ui_page_boot_reset_key_state(void) {
    key_enter_first_press = false;
    force_pwr_activated = false;
}

void ui_page_boot_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_ENTER) {
        // OLEDがオフになっている場合は、表示を復帰
        if (oled_cleared) {
            OLED_display(1);
            oled_cleared = false;
            sleep_start_ms = SysTick->CNT / (F_CPU / 1000);  // スリープタイマーをリセット
            return;
        }

        uint32_t current_ms = SysTick->CNT / (F_CPU / 1000);

        if (!key_enter_first_press) {
            // 最初の押下
            key_enter_press_start_ms = current_ms;
            key_enter_first_press = true;
            force_pwr_activated = false;
        } else if (!force_pwr_activated) {
            // リピート中（押し続けている）
            uint32_t duration_ms = current_ms - key_enter_press_start_ms;
            if (duration_ms >= 3000) {
                // 3秒以上長押し
                if (!pctx->ctx->power_on) {
                    // 電源が入っていない場合は、強制パワーオン（画面遷移はmainループで行われる）
                    set_force_pwr_on(true);
                    force_pwr_activated = true;
                }
                key_enter_first_press = false;  // 次の押下を待つ
            }
        }
    }
}