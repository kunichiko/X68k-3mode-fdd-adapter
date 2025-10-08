#include "greenpak/greenpak_control.h"
#include "pcfdd/pcfdd_control.h"
#include "power/power_control.h"
#include "ui_control.h"

// boot page
static void ui_page_boot_enter(ui_page_context_t* pctx);
void ui_page_boot_poll(ui_page_context_t* pctx, uint32_t systick_ms);
void ui_page_boot_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

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
    static bool last_enter_state = false;

    if (ui_get_current_page() != UI_PAGE_BOOT) {
        return;
    }
    ui_page_type_t page = UI_PAGE_BOOT;
    minyasx_context_t* ctx = pctx->ctx;

    // ENTERキーの状態を監視してリリース時にフラグをリセット
    extern const uint8_t gp_target_addr[5];
    uint8_t gp5_io0_7 = gp_reg_get(gp_target_addr[4], 0x74);
    bool enter_pressed = ((gp5_io0_7 & (1 << 5)) == 0);

    if (last_enter_state && !enter_pressed) {
        // ENTERキーが離された
        extern void ui_page_boot_reset_key_state(void);
        ui_page_boot_reset_key_state();
    }
    last_enter_state = enter_pressed;

    // 表示のみを行う（画面遷移はmainループで管理）
    if (!ctx->power_on) {
        ui_cursor(page, 0, 3);
        ui_print(page, "    - Sleeping -\n");
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
            }
        }
    }
}