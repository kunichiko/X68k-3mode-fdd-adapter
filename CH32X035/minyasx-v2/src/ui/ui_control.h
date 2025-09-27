#ifndef UI_CONTROL_H
#define UI_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minyasx.h"
#include "oled/ssd1306_txt.h"

typedef enum {
    UI_PAGE_BOOT = 0,            // Boot page
    UI_PAGE_MAIN = 1,            // Main page
    UI_PAGE_MENU = 2,            // Menu page
    UI_PAGE_ABOUT = 3,           // About page
    UI_PAGE_PDSTATUS = 4,        // PD status page
    UI_PAGE_SETTING_COMMON = 5,  // Common settings page
    UI_PAGE_SETTING_FDDA = 6,    // FD Drive A settings page
    UI_PAGE_SETTING_FDDB = 7,    // FD Drive B settings page
    UI_PAGE_DEBUG = 8,           // Debug page
    UI_PAGE_DEBUG_PCFDD = 9,     // PCFDD debug page
    UI_PAGE_LOG = 10,            // Log page
    UI_PAGE_MAX,
} ui_page_type_t;

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
// ui_page_context_tのプロトタイプ宣言
struct ui_page_context_t;  // 構造体の前方宣言

typedef void (*ui_page_enter_t)(struct ui_page_context_t* pctx);
typedef void (*ui_page_poll_t)(struct ui_page_context_t* pctx, uint32_t systick_ms);
typedef void (*ui_page_keyin_t)(struct ui_page_context_t* pctx, ui_key_mask_t keys);

typedef struct ui_page_context_t {
    minyasx_context_t* ctx;
    ui_page_type_t page;
    uint8_t buf[8][21];  // 128x64dot with 6x8font = 21x8char
    uint8_t x;
    uint8_t y;
    bool scroll_enable;      // true: 画面下端でスクロールする, false: しない
    bool scroll_keepheader;  // true: スクロール時にヘッダー行を維持する
    ui_page_enter_t enter;
    ui_page_poll_t poll;
    ui_page_keyin_t keyin;
} ui_page_context_t;

void ui_change_page(ui_page_type_t page);
ui_page_type_t ui_get_current_page(void);

void ui_page_main_init(ui_page_context_t* win);
void ui_page_menu_init(ui_page_context_t* win);
void ui_page_about_init(ui_page_context_t* win);
void ui_page_pdstatus_init(ui_page_context_t* win);
void ui_page_setting_common_init(ui_page_context_t* win);
void ui_page_setting_fdda_init(ui_page_context_t* win);
void ui_page_setting_fddb_init(ui_page_context_t* win);
void ui_page_debug_init(ui_page_context_t* win);
void ui_page_debug_init_pcfdd(ui_page_context_t* win);
void ui_page_log_init(ui_page_context_t* win);

typedef void (*ui_write_t)(char c);  // Write a character or handle control characters

ui_write_t ui_get_writer(ui_page_type_t page);

void ui_clear(ui_page_type_t page);
void ui_cursor(ui_page_type_t page, uint8_t x, uint8_t y);
void ui_print(ui_page_type_t page, char* str);
void ui_write(ui_page_type_t page, char c);

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

// 複数の選択肢を上下キーで選択し、Enterキーで決定するUIを表示する
// 選択肢はNULL終端の文字列配列で与える
// 戻り値は選択されたインデックス（0から始まる）
// キーのコールバックを受け渡す必要があるので、以下の関数に分解する
// - ui_select_init() : 選択肢の表示と初期化
// - ui_select_keyin() : キー入力のコールバック
typedef struct {
    ui_page_type_t page;   // 選択肢を表示するページ
    int x;                 // 選択肢の表示位置X
    int y;                 // 選択肢の表示位置Y
    int width;             // 選択肢の表示幅（最大文字数）
    const char** options;  // 選択肢の文字列配列（NULL終端）
    size_t option_count;   // 選択肢の数
    size_t current_index;  // 現在選択されているインデックス
    bool selection_made;   // 選択が確定したか
} ui_select_t;
void ui_select_init(ui_select_t* select);
void ui_select_keyin(ui_select_t* select, ui_key_mask_t keys);

#endif  // UI_CONTROL_H