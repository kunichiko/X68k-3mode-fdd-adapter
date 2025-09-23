#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minyasx.h"
#include "oled/ssd1306_txt.h"

typedef enum {
    UI_PAGE_MAIN = 0,            // Main page
    UI_PAGE_MENU = 1,            // Menu page
    UI_PAGE_ABOUT = 2,           // About page
    UI_PAGE_PDSTATUS = 3,        // PD status page
    UI_PAGE_SETTING_COMMON = 4,  // Common settings page
    UI_PAGE_SETTING_FDDA = 5,    // FD Drive A settings page
    UI_PAGE_SETTING_FDDB = 6,    // FD Drive B settings page
    UI_PAGE_DEBUG = 7,           // Debug page
    UI_PAGE_DEBUG_PCFDD = 8,     // PCFDD debug page
    UI_PAGE_MAX,
} UI_PAGE_t;

typedef enum {
    UI_KEY_NONE = 0,          // 何も押されていない
    UI_KEY_UP = 1 << 0,       // 00000001
    UI_KEY_DOWN = 1 << 1,     // 00000010
    UI_KEY_LEFT = 1 << 2,     // 00000100
    UI_KEY_RIGHT = 1 << 3,    // 00001000
    UI_KEY_ENTER = 1 << 4,    // 00010000
    UI_KEY_EJECT_A = 1 << 5,  // 00100000
    UI_KEY_EJECT_B = 1 << 6   // 01000000
} ui_key_t;

typedef uint32_t ui_key_mask_t;  // 同時押し表現用

// コールバック関数の型（キーが押されたら呼ばれる）
typedef void (*ui_key_callback_t)(ui_key_mask_t keys);

typedef struct {
    UI_PAGE_t page;
    uint8_t buf[8][21];  // 128x64dot with 6x8font = 21x8char
    uint8_t x;
    uint8_t y;
    ui_key_callback_t key_callback;
} ui_window_t;

void ui_change_page(UI_PAGE_t page);
UI_PAGE_t ui_get_current_page(void);
typedef void (*ui_write_t)(char c);  // Write a character or handle control characters

ui_write_t ui_get_writer(UI_PAGE_t page);

void ui_clear(UI_PAGE_t page);
void ui_cursor(UI_PAGE_t page, uint8_t x, uint8_t y);
void ui_print(UI_PAGE_t page, char* str);
void ui_write(UI_PAGE_t page, char c);

#include "print.h"
#define ui_printD(p, n) printD(ui_get_writer(p), n)  // print decimal as string
#define ui_printW(p, n) printW(ui_get_writer(p), n)  // print word as string
#define ui_printH(p, n) printH(ui_get_writer(p), n)  // print half-word as string
#define ui_printB(p, n) printB(ui_get_writer(p), n)  // print byte as string
#define ui_printS(s) printS(ui_get_writer(p), s)     // print string
#define ui_println(s) println(ui_get_writer(p), s)   // print string with newline
#define ui_newline() ui_get_writer(p)('\n')          // send newline
#define ui_printf(p, f, ...) printF(ui_get_writer(p), f, ##__VA_ARGS__)

void ui_init(minyasx_context_t* ctx);

void ui_poll(minyasx_context_t* ctx, uint32_t systick_ms);

// main page
void ui_page_main_init(minyasx_context_t* ctx, ui_window_t* win);
void ui_page_main_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void ui_page_main_key_callback(ui_key_mask_t keys);
// menu page
void ui_page_menu_init(minyasx_context_t* ctx, ui_window_t* win);
void ui_page_menu_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void ui_page_menu_key_callback(ui_key_mask_t keys);
// about page
void ui_page_about_init(minyasx_context_t* ctx, ui_window_t* win);
void ui_page_about_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void ui_page_about_key_callback(ui_key_mask_t keys);
// pdstatus page
void ui_page_pdstatus_init(minyasx_context_t* ctx, ui_window_t* win);
void ui_page_pdstatus_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void ui_page_pdstatus_key_callback(ui_key_mask_t keys);

// debug page
void ui_page_debug_init(minyasx_context_t* ctx, ui_window_t* win);
void ui_page_debug_init_pcfdd(minyasx_context_t* ctx, ui_window_t* win);
void ui_page_debug_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void ui_page_debug_key_callback(ui_key_mask_t keys);
void ui_page_debug_key_callback_pcfdd(ui_key_mask_t keys);

#endif  // UI_CONTROL_H