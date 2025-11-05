#ifndef UI_LOG_BUFFER_H
#define UI_LOG_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

// ログバッファの管理構造体
// 最大100行のログをリングバッファで保持し、バックスクロール機能を提供
typedef struct {
    char lines[100][21];  // 100行×21文字のリングバッファ
    uint16_t head;        // 最新行のインデックス（0-99）
    uint16_t count;       // 格納されている行数（0-100）
    int16_t view_offset;  // 表示オフセット（0=最新、負数=過去）
    bool scroll_locked;   // trueの場合、ユーザーがバックスクロール中
} ui_log_buffer_t;

// ログバッファの初期化
void ui_log_buffer_init(ui_log_buffer_t* buf);

// ログバッファに1行追加（最大21文字、NULL終端）
void ui_log_buffer_add_line(ui_log_buffer_t* buf, const char* line);

// 現在の表示位置から7行分を取得
// lines[0]が最も古い行、lines[6]が最も新しい行
void ui_log_buffer_get_view(ui_log_buffer_t* buf, char lines[7][21]);

// 古い方へスクロール（画面を上にスクロール）
void ui_log_buffer_scroll_up(ui_log_buffer_t* buf);

// 新しい方へスクロール（画面を下にスクロール）
void ui_log_buffer_scroll_down(ui_log_buffer_t* buf);

// スクロールロックを解除し、最新に戻る
void ui_log_buffer_unlock(ui_log_buffer_t* buf);

// 現在の表示オフセットを取得（0=最新、負数=何行前）
int16_t ui_log_buffer_get_offset(ui_log_buffer_t* buf);

#endif  // UI_LOG_BUFFER_H
