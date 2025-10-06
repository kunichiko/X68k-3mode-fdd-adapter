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
    if (ui_get_current_page() != UI_PAGE_BOOT) {
        return;
    }
    ui_page_type_t page = UI_PAGE_BOOT;
    minyasx_context_t* ctx = pctx->ctx;
    ui_cursor(page, 0, 3);
    if (ctx->power_on) {
        ui_print(page, "                \n");
        ui_change_page(UI_PAGE_MAIN);
    } else {
        ui_print(page, "    - Sleeping -\n");
    }
}

void ui_page_boot_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    static uint32_t last_key_enter_systick = 0;
    if (keys & UI_KEY_ENTER) {
        // Enterキーが押された場合
        uint32_t systick = SysTick->CNT;
        uint32_t ms = systick / (F_CPU / 1000);
        if (ms - last_key_enter_systick < 1000) {
            // 1秒以内に連続で押されたら場合
            // 電源が入っていなければ強制パワーオンする
            if (!pctx->ctx->power_on) {
                // 電源が入っていない場合は、強制パワーオン
                set_force_pwr_on(true);
                ui_change_page(UI_PAGE_MAIN);
                return;
            }
        }
        last_key_enter_systick = ms;
    }
}