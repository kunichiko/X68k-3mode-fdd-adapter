#include "ui_page_log.h"

#include <stdio.h>
#include <string.h>

#include "ui_control.h"
#include "ui_log_buffer.h"

// ログバッファのインスタンス
static ui_log_buffer_t log_buffer;

// 1行分の文字バッファリング用
static char current_line[21];
static uint8_t current_line_pos = 0;

// 初期化済みフラグ
static bool initialized = false;

// 前方宣言
void ui_page_log_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);
void ui_page_log_poll(ui_page_context_t* pctx, uint32_t systick_ms);

void ui_page_log_init(ui_page_context_t* pcon) {
    pcon->enter = NULL;
    pcon->poll = ui_page_log_poll;
    pcon->keyin = ui_page_log_keyin;
    pcon->scroll_enable = false;      // バックスクロール機能を使うので自動スクロールは無効
    pcon->scroll_keepheader = false;  // 手動で管理するので無効

    // ログバッファの初期化（初回のみ）
    if (!initialized) {
        ui_log_buffer_init(&log_buffer);
        memset(current_line, 0, sizeof(current_line));
        current_line_pos = 0;
        initialized = true;
    }

    // 初期表示
    ui_page_log_refresh(pcon);
}

void ui_page_log_refresh(ui_page_context_t* pctx) {
    // バッファから7行分を取得
    char view_lines[7][21];
    ui_log_buffer_get_view(&log_buffer, view_lines);

    // ヘッダー行の作成
    int16_t offset = ui_log_buffer_get_offset(&log_buffer);
    char header[22];
    if (offset == 0) {
        snprintf(header, 22, "====[Log: Latest]===");
    } else {
        snprintf(header, 22, "====[Log -%3d]====", -offset);
    }

    // バッファをクリア
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 21; x++) {
            pctx->buf[y][x] = ' ';
        }
    }

    // ヘッダー行を書き込み
    for (int x = 0; x < 20 && header[x] != '\0'; x++) {
        pctx->buf[0][x] = header[x];
    }

    // ログ行を書き込み（1-7行目）
    for (int i = 0; i < 7; i++) {
        for (int x = 0; x < 20 && view_lines[i][x] != '\0'; x++) {
            pctx->buf[i + 1][x] = view_lines[i][x];
        }
    }

    // 現在のページの場合、OLEDに反映
    if (ui_get_current_page() == pctx->page) {
        // OLEDを更新
        for (int y = 0; y < 8; y++) {
            OLED_cursor(0, y);
            for (int x = 0; x < 21; x++) {
                OLED_write(pctx->buf[y][x]);
            }
        }
    }
}

void ui_page_log_poll(ui_page_context_t* pctx, uint32_t systick_ms) {
    // 定期的に表示を更新（スクロールロックされていない場合のみ）
    static uint32_t last_refresh = 0;
    if (!log_buffer.scroll_locked && (systick_ms - last_refresh > 100)) {
        last_refresh = systick_ms;
        if (ui_get_current_page() == pctx->page) {
            ui_page_log_refresh(pctx);
        }
    }
}

void ui_page_log_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    bool need_refresh = false;

    if (keys & UI_KEY_UP) {
        // 古い方へスクロール
        ui_log_buffer_scroll_up(&log_buffer);
        need_refresh = true;
    }
    if (keys & UI_KEY_DOWN) {
        // 新しい方へスクロール
        ui_log_buffer_scroll_down(&log_buffer);
        need_refresh = true;
    }
    if (keys & UI_KEY_RIGHT) {
        // メインページに戻る
        ui_change_page(UI_PAGE_MAIN);
    }
    if (keys & UI_KEY_LEFT) {
        // デバッグページに遷移
        ui_change_page(UI_PAGE_DEBUG);
    }
    if (keys & UI_KEY_ENTER) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
    }

    if (need_refresh) {
        ui_page_log_refresh(pctx);
    }
}

void ui_page_log_write_char(char c) {
    // 1行分のバッファリング
    if (c == '\n') {
        // 改行が来たら1行をログバッファに追加
        current_line[current_line_pos] = '\0';
        ui_log_buffer_add_line(&log_buffer, current_line);

        // バッファをクリア
        memset(current_line, 0, sizeof(current_line));
        current_line_pos = 0;

        // 現在のページがLOGページでスクロールロックされていない場合、即座に更新
        if (ui_get_current_page() == UI_PAGE_LOG && !log_buffer.scroll_locked) {
            ui_refresh_log_page();
        }
    } else if (c == '\r') {
        // キャリッジリターンは行の先頭に戻る
        current_line_pos = 0;
    } else {
        // 通常の文字
        if (current_line_pos < 20) {
            current_line[current_line_pos] = c;
            current_line_pos++;
        } else {
            // 行が21文字を超えた場合は自動改行
            current_line[20] = '\0';
            ui_log_buffer_add_line(&log_buffer, current_line);

            // 新しい行を開始
            memset(current_line, 0, sizeof(current_line));
            current_line[0] = c;
            current_line_pos = 1;

            // 即座に更新
            if (ui_get_current_page() == UI_PAGE_LOG && !log_buffer.scroll_locked) {
                ui_refresh_log_page();
            }
        }
    }
}
