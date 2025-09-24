#include "ui_control.h"

#include "greenpak/greenpak_control.h"

static ui_page_context_t ui_pages[UI_PAGE_MAX];

static ui_page_type_t current_page = UI_PAGE_MAIN;

void ui_refresh(void) {
    // 現在のページに対応するバッファを取得
    ui_page_context_t *win = &ui_pages[current_page];
    // OLEDをバッファの内容で更新
    for (int y = 0; y < 8; y++) {
        OLED_cursor(0, y);
        for (int x = 0; x < 21; x++) {
            char c = win->buf[y][x];
            OLED_write(c);
        }
    }
}

void ui_change_page(ui_page_type_t page) {
    if (page < 0 || page >= UI_PAGE_MAX) {
        return;  // 無効なページ番号
    }
    if (current_page == page) {
        return;  // すでにそのページがアクティブ
    }
    // ページを変更
    current_page = page;
    // 画面を更新
    ui_refresh();
    // ページのenterコールバックを呼び出す
    if (ui_pages[page].enter) {
        ui_pages[page].enter(&ui_pages[page]);
    }
}

ui_page_type_t ui_get_current_page(void) {
    return current_page;
}

void ui_clear(ui_page_type_t page) {
    if (page < 0 || page >= UI_PAGE_MAX) {
        return;  // 無効なページ番号
    }
    ui_page_context_t *win = &ui_pages[page];
    // バッファをクリア
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 21; x++) {
            win->buf[y][x] = ' ';  // スペースでクリア
        }
    }
    win->x = 0;
    win->y = 0;
    if (current_page == page) {
        // 現在のページがアクティブならOLEDもクリア
        OLED_clear();
        OLED_cursor(0, 0);
    }
}

void ui_cursor(ui_page_type_t page, uint8_t x, uint8_t y) {
    if (page < 0 || page >= UI_PAGE_MAX) {
        return;  // 無効なページ番号
    }
    ui_page_context_t *win = &ui_pages[page];
    if (x < 0) x = 0;
    if (x >= 21) x = 20;
    if (y < 0) y = 0;
    if (y >= 8) y = 7;
    win->x = x;
    win->y = y;
    if (current_page == page) {
        OLED_cursor(x * 6, y);
    }
}

void ui_print(ui_page_type_t page, char *str) {
    while (*str) ui_write(page, *str++);
}

void ui_write(ui_page_type_t page, char c) {
    // 現在のページに対応するバッファを取得
    ui_page_context_t *win = &ui_pages[page];
    // バッファに文字を書き込む
    if (c == '\n') {
        win->x = 0;
        win->y++;
        if (win->y >= 8) {
            win->y = 0;  // 簡易的にスクロールせずに戻す
        }
    } else if (c == '\r') {
        win->x = 0;
    } else {
        if (win->x < 21 && win->y < 8) {
            win->buf[win->y][win->x] = c;
            if (current_page == page) {
                // 現在のページがアクティブならOLEDに反映
                OLED_cursor(win->x * 6, win->y);  // Xは6ドット幅で計算
                OLED_write(c);
            }
            win->x++;
            if (win->x >= 21) {
                win->x = 0;
                win->y++;
                if (win->y >= 8) {
                    win->y = 0;  // 簡易的にスクロールせずに戻す
                }
            }
        }
    }
    return;
}

void ui_write_0(char c) {
    ui_write(0, c);
}
void ui_write_1(char c) {
    ui_write(1, c);
}
void ui_write_2(char c) {
    ui_write(2, c);
}
void ui_write_3(char c) {
    ui_write(3, c);
}
void ui_write_4(char c) {
    ui_write(4, c);
}
void ui_write_5(char c) {
    ui_write(5, c);
}
void ui_write_6(char c) {
    ui_write(6, c);
}
void ui_write_7(char c) {
    ui_write(7, c);
}
void ui_write_8(char c) {
    ui_write(8, c);
}
void ui_write_9(char c) {
    ui_write(9, c);
}
void ui_write_10(char c) {
    ui_write(10, c);
}
void ui_write_null(char c) {
    // 何もしない
    (void)c;
}

ui_write_t writers[UI_PAGE_MAX] = {
    ui_write_0, ui_write_1, ui_write_2,  ui_write_3,  //
    ui_write_4, ui_write_5, ui_write_6,  ui_write_7,  //
    ui_write_8, ui_write_9, ui_write_10,
};

ui_write_t ui_get_writer(ui_page_type_t page) {
    // 現在のページに対応するライター関数を返す
    if (page < 0 || page >= UI_PAGE_MAX) {
        return ui_write_null;
    }
    return writers[page];
}

void ui_init(minyasx_context_t *ctx) {
    // 各ウィンドウの初期化
    for (int i = 0; i < UI_PAGE_MAX; i++) {
        ui_pages[i].ctx = ctx;
        ui_pages[i].page = (ui_page_type_t)i;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 21; x++) {
                ui_pages[i].buf[y][x] = ' ';  // バッファをクリア
            }
        }
        ui_pages[i].x = 0;
        ui_pages[i].y = 0;
        ui_pages[i].enter = NULL;
        ui_pages[i].poll = NULL;
        ui_pages[i].keyin = NULL;
    }
    // OLEDの初期化
    OLED_init();
    OLED_display(1);  // ディスプレイをオンにする
    OLED_flip(1, 1);  // 必要に応じて画面を反転
    OLED_clear();     // 画面をクリア
    current_page = UI_PAGE_MAIN;

    // 各ページの初期化
    ui_page_main_init(&ui_pages[UI_PAGE_MAIN]);
    ui_page_menu_init(&ui_pages[UI_PAGE_MENU]);
    ui_page_about_init(&ui_pages[UI_PAGE_ABOUT]);
    ui_page_pdstatus_init(&ui_pages[UI_PAGE_PDSTATUS]);
    ui_page_setting_common_init(&ui_pages[UI_PAGE_SETTING_COMMON]);
    ui_page_setting_fdda_init(&ui_pages[UI_PAGE_SETTING_FDDA]);
    ui_page_setting_fddb_init(&ui_pages[UI_PAGE_SETTING_FDDB]);
    ui_page_debug_init(&ui_pages[UI_PAGE_DEBUG]);
    ui_page_debug_init_pcfdd(&ui_pages[UI_PAGE_DEBUG_PCFDD]);
}

void ui_poll(minyasx_context_t *ctx, uint32_t systick_ms) {
    // 各ページのポーリング処理
    for (int i = 0; i < UI_PAGE_MAX; i++) {
        ui_page_context_t *win = &ui_pages[i];
        if (win->poll) {
            win->poll(win, systick_ms);
        }
    }

    // ここでキー入力のポーリングを行い、必要に応じてコールバックを呼び出す
    // 例えば、キー状態を読み取る関数があると仮定
    ui_key_mask_t keys = UI_KEY_NONE;
    static ui_key_mask_t last_keys = UI_KEY_NONE;

    // キー入力はGP4のIO端子をI2Cで読める
    // GPのIO端子の入力状態はレジスタ0x74,0x75で読める
    // 0x74:
    // - bit1: IO0 (UP)
    // - bit2: IO1 (DOWN)
    // - bit3: IO2 (LEFT)
    // - bit4: IO3 (RIGHT)
    // - bit5: IO4 (ENTER)
    // 0x75:
    // - bit4: IO13 (EJECT_B)
    // - bit5: IO14 (EJECT_A)
    uint8_t gp4_io0_7 = gp_reg_get(gp_target_addr[3], 0x74);
    uint8_t gp4_io8_15 = gp_reg_get(gp_target_addr[3], 0x75);
    if ((gp4_io0_7 & (1 << 1)) == 0) keys |= UI_KEY_UP;
    if ((gp4_io0_7 & (1 << 2)) == 0) keys |= UI_KEY_DOWN;
    if ((gp4_io0_7 & (1 << 3)) == 0) keys |= UI_KEY_LEFT;
    if ((gp4_io0_7 & (1 << 4)) == 0) keys |= UI_KEY_RIGHT;
    if ((gp4_io0_7 & (1 << 5)) == 0) keys |= UI_KEY_ENTER;
    if ((gp4_io8_15 & (1 << 4)) == 0) keys |= UI_KEY_EJECT_B;
    if ((gp4_io8_15 & (1 << 5)) == 0) keys |= UI_KEY_EJECT_A;

    if (keys == last_keys) {
        // キー状態が変化していない
        return;
    }
    last_keys = keys;
    ui_page_context_t *page = &ui_pages[current_page];
    if (page->keyin) {
        page->keyin(&ui_pages[current_page], keys);
    }
}

void ui_select_print(ui_select_t *select, bool inverted) {
    // 選択肢の表示
    if (select->options && select->current_index < select->option_count) {
        uint8_t str[select->width + 1];
        str[select->width] = 0;  // NULL終端
        bool end = false;
        for (int i = 0; i < select->width; i++) {
            if (!end && select->options[select->current_index][i] != 0) {
                str[i] = select->options[select->current_index][i] | (inverted ? 0x80 : 0);
            } else {
                end = true;
                str[i] = ' ' | (inverted ? 0x80 : 0);  // 空白で初期化
            }
        }
        ui_cursor(select->page, select->x, select->y);
        ui_print(select->page, (char *)str);
    }
}

void ui_select_init(ui_select_t *select) {
    // 現在の選択肢を表示
    ui_select_print(select, true);
}

/**
 * 上下キーで選択肢を移動し、Enterキーで決定する
 * 選択が確定したら select->selection_made を true にする
 * keys: 押されたキーのビットマスク
 */
void ui_select_keyin(ui_select_t *select, ui_key_mask_t keys) {
    if (keys & UI_KEY_UP) {
        // 上キーが押された場合
        if (select->current_index > 0) {
            select->current_index--;
        }
    } else if (keys & UI_KEY_DOWN) {
        // 下キーが押された場合
        if (select->current_index < select->option_count - 1) {
            select->current_index++;
        }
    } else if (keys & UI_KEY_ENTER) {
        // Enterキーが押された場合
        select->selection_made = true;
        ui_select_print(select, false);  // 選択確定なので反転解除
        return;
    }
    // 選択肢の表示を更新
    ui_select_print(select, true);
}