#include "ui_control.h"

#include "greenpak/greenpak_control.h"

static ui_window_t ui_windows[UI_MAX_WINDOWS];

static UI_PAGE_t current_page = UI_PAGE_MAIN;

void ui_refresh(void) {
    // 現在のページに対応するバッファを取得
    ui_window_t *win = &ui_windows[current_page];
    // OLEDをクリアしてバッファの内容を描画
    OLED_clear();
    for (int y = 0; y < 8; y++) {
        OLED_cursor(0, y);
        for (int x = 0; x < 21; x++) {
            char c = win->buf[y][x];
            if (c != 0) {
                OLED_write(c);
            } else {
                break;  // NULL文字で終了
            }
        }
    }
}

void ui_change_page(UI_PAGE_t page) {
    if (page < 0 || page >= UI_MAX_WINDOWS) {
        return;  // 無効なページ番号
    }
    if (current_page == page) {
        return;  // すでにそのページがアクティブ
    }
    // ページを変更
    current_page = page;
    // 画面を更新
    ui_refresh();
}

void ui_clear(UI_PAGE_t page) {
    if (page < 0 || page >= UI_MAX_WINDOWS) {
        return;  // 無効なページ番号
    }
    ui_window_t *win = &ui_windows[page];
    // バッファをクリア
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 21; x++) {
            win->buf[y][x] = 0;
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

void ui_cursor(UI_PAGE_t page, uint8_t x, uint8_t y) {
    if (page < 0 || page >= UI_MAX_WINDOWS) {
        return;  // 無効なページ番号
    }
    ui_window_t *win = &ui_windows[page];
    if (x < 0) x = 0;
    if (x >= 21) x = 20;
    if (y < 0) y = 0;
    if (y >= 8) y = 7;
    win->x = x;
    win->y = y;
    if (current_page == page) {
        OLED_cursor(x, y);
    }
}

void ui_print(UI_PAGE_t page, char *str) {
    while (*str) ui_write(page, *str++);
}

void ui_write(UI_PAGE_t page, char c) {
    // 現在のページに対応するバッファを取得
    ui_window_t *win = &ui_windows[page];
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
void ui_write_null(char c) {
    // 何もしない
    (void)c;
}

ui_write_t ui_get_writer(UI_PAGE_t page) {
    // 現在のページに対応するライター関数を返す
    switch (page) {
    case UI_PAGE_MAIN:
        return ui_write_0;
    case UI_PAGE_MENU:
        return ui_write_1;
    case UI_PAGE_ABOUT:
        return ui_write_2;
    case UI_PAGE_TOOLTIP:
        return ui_write_3;
    case UI_PAGE_CUSTOM:
        return ui_write_4;
    default:
        return ui_write_null;
    }
}

void ui_init(minyasx_context_t *ctx) {
    // 各ウィンドウの初期化
    for (int i = 0; i < UI_MAX_WINDOWS; i++) {
        ui_windows[i].page = (UI_PAGE_t)i;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 21; x++) {
                ui_windows[i].buf[y][x] = 0;  // バッファをクリア
            }
        }
        ui_windows[i].x = 0;
        ui_windows[i].y = 0;
        ui_windows[i].key_callback = NULL;
    }
    // OLEDの初期化
    OLED_init();
    OLED_display(1);  // ディスプレイをオンにする
    OLED_flip(1, 1);  // 必要に応じて画面を反転
    OLED_clear();     // 画面をクリア
    current_page = UI_PAGE_MAIN;

    // 各ページの初期化
    ui_page_main_init(ctx, &ui_windows[UI_PAGE_MAIN]);
    ui_page_menu_init(ctx, &ui_windows[UI_PAGE_MENU]);
    ui_page_about_init(ctx, &ui_windows[UI_PAGE_ABOUT]);
}

void ui_poll(minyasx_context_t *ctx, uint32_t systick_ms) {
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
    ui_window_t *win = &ui_windows[current_page];
    if (win->key_callback) {
        win->key_callback(keys);
    }
}