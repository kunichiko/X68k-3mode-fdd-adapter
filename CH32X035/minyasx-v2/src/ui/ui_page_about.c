#include "ui_control.h"

// About page
static void ui_page_about_enter(ui_page_context_t* pctx);
void ui_page_about_poll(ui_page_context_t* pctx, uint32_t systick_ms);
void ui_page_about_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

void ui_page_about_init(ui_page_context_t* win) {
    win->enter = ui_page_about_enter;
    win->poll = NULL;
    win->keyin = ui_page_about_keyin;

    ui_page_type_t page = win->page;

    ui_cursor(page, 0, 0);
    ui_print(page, "[About]\n");
    ui_print(page, " Minyas X @kunichiko\n");
    ui_print(page, " PCB Version 2.0.2\n");
    ui_print(page, " Firm Version 2.0.0\n");
    ui_print(page, " FDD A:\n");
    ui_print(page, " FDD B:\n");
    ui_cursor(page, 0, 7);
    ui_print(page, ">RETURN");
}

void ui_page_about_enter(ui_page_context_t* pctx) {
    ui_page_type_t page = pctx->page;
    minyasx_context_t* ctx = pctx->ctx;
    // Aboutページのポーリング処理
    for (int i = 0; i < 2; i++) {
        ui_cursor(page, 7, 4 + i);
        switch (ctx->drive[i].state) {
        case DRIVE_STATE_POWER_OFF:
            ui_print(page, "Power Off ");
            break;
        case DRIVE_STATE_NOT_CONNECTED:
            ui_print(page, "No Drive  ");
            break;
        case DRIVE_STATE_INITIALIZING:
            ui_print(page, "Init...   ");
            break;
        case DRIVE_STATE_POWER_ON:
            ui_print(page, "Ready     ");
            break;
        default:
            ui_print(page, "Unknown   ");
            break;
        }
    }
}

void ui_page_about_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    if (keys & UI_KEY_LEFT) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
        return;
    }
    if (keys & UI_KEY_ENTER) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
        return;
    }
}