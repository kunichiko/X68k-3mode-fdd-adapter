#include "ui_log_buffer.h"

#include <string.h>

void ui_log_buffer_init(ui_log_buffer_t* buf) {
    // すべてのフィールドを初期化
    memset(buf->lines, 0, sizeof(buf->lines));
    buf->head = 0;
    buf->count = 0;
    buf->view_offset = 0;
    buf->scroll_locked = false;
}

void ui_log_buffer_add_line(ui_log_buffer_t* buf, const char* line) {
    // 新しい行を追加（リングバッファの次の位置へ）
    buf->head = (buf->head + 1) % 100;

    // 行をコピー（最大20文字 + NULL終端）
    strncpy(buf->lines[buf->head], line, 20);
    buf->lines[buf->head][20] = '\0';  // NULL終端を保証

    // カウントを更新（最大100）
    if (buf->count < 100) {
        buf->count++;
    }

    // スクロールロック中の場合、表示範囲のチェック
    if (buf->scroll_locked) {
        // 表示中の最古行のオフセット（view_offsetから6行前）
        int16_t oldest_displayed = buf->view_offset - 6;

        // バッファ内で利用可能な最古行のオフセット
        int16_t oldest_available = -(int16_t)(buf->count - 1);

        // 表示中の最古行が押し出される場合
        if (oldest_displayed < oldest_available) {
            // 表示位置を1行新しい方へずらす
            buf->view_offset++;

            // 最新に達したらロック解除
            if (buf->view_offset >= 0) {
                buf->scroll_locked = false;
                buf->view_offset = 0;
            }
        }
    }
}

void ui_log_buffer_get_view(ui_log_buffer_t* buf, char lines[7][21]) {
    // 表示する7行分を取得
    // view_offset = 0 の場合、最新から7行
    // view_offset = -10 の場合、10行前から7行

    // すべての行をクリア
    for (int i = 0; i < 7; i++) {
        memset(lines[i], ' ', 20);
        lines[i][20] = '\0';
    }

    // バッファが空の場合は空行を返す
    if (buf->count == 0) {
        return;
    }

    // 表示開始位置を計算（最新行からのオフセット）
    int16_t start_offset = buf->view_offset - 6;

    // 7行分をコピー
    for (int i = 0; i < 7; i++) {
        int16_t line_offset = start_offset + i;

        // オフセットが有効範囲内かチェック
        // 有効範囲: -(count-1) <= line_offset <= 0
        if (line_offset >= -(int16_t)(buf->count - 1) && line_offset <= 0) {
            // リングバッファ内のインデックスを計算
            int16_t index = (int16_t)buf->head + line_offset;
            if (index < 0) {
                index += 100;
            }
            index = index % 100;

            // 行をコピー
            strncpy(lines[i], buf->lines[index], 20);
            lines[i][20] = '\0';
        }
    }
}

void ui_log_buffer_scroll_up(ui_log_buffer_t* buf) {
    // 古い方へスクロール（画面を上に移動）

    // バッファが7行以下の場合はスクロールしない
    if (buf->count <= 7) {
        return;
    }

    // スクロールロックを有効化
    buf->scroll_locked = true;

    // オフセットを減らす（古い方へ）
    buf->view_offset--;

    // 最古行を超えないように制限
    int16_t oldest_offset = -(int16_t)(buf->count - 7);
    if (buf->view_offset < oldest_offset) {
        buf->view_offset = oldest_offset;
    }
}

void ui_log_buffer_scroll_down(ui_log_buffer_t* buf) {
    // 新しい方へスクロール（画面を下に移動）

    // すでに最新の場合は何もしない
    if (buf->view_offset >= 0) {
        buf->scroll_locked = false;
        buf->view_offset = 0;
        return;
    }

    // オフセットを増やす（新しい方へ）
    buf->view_offset++;

    // 最新に達したらロック解除
    if (buf->view_offset >= 0) {
        buf->scroll_locked = false;
        buf->view_offset = 0;
    }
}

void ui_log_buffer_unlock(ui_log_buffer_t* buf) {
    // スクロールロックを解除し、最新に戻る
    buf->scroll_locked = false;
    buf->view_offset = 0;
}

int16_t ui_log_buffer_get_offset(ui_log_buffer_t* buf) {
    // 現在の表示オフセットを返す
    return buf->view_offset;
}
