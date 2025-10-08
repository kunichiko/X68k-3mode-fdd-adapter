#include "power/power_control.h"
#include "preferences/preferences_control.h"
#include "ui/ui_control.h"

// Common Setting page
static void ui_page_setting_common_enter(ui_page_context_t* pctx);
static void ui_page_setting_common_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);

// Speaker選択用UI
static const char* speaker_options[] = {"On", "Off", NULL};
ui_select_t speaker_select = {
    .page = UI_PAGE_SETTING_COMMON,
    .x = 11,
    .y = 1,
    .width = 4,
    .options = speaker_options,
    .option_count = 2,
    .current_index = 0,
    .selection_made = true,  // falseで選択モードに入る
};

// AutoDetect選択用UI
static const char* auto_detect_options[] = {"Off", "60s", "Keep", NULL};
ui_select_t auto_detect_select = {
    .page = UI_PAGE_SETTING_COMMON,
    .x = 11,
    .y = 2,
    .width = 4,
    .options = auto_detect_options,
    .option_count = 3,
    .current_index = 0,
    .selection_made = true,  // falseで選択モードに入る
};

// Reset Defaults確認ダイアログ
ui_dialog_t reset_dialog = {
    .page = UI_PAGE_SETTING_COMMON,
    .message = "Reset?",
    .dialog_open = false,
    .result = false,
    .selection_made = false,
    .selected_button = 1,  // デフォルトはCancel
};

void ui_page_setting_common_init(ui_page_context_t* win) {
    win->enter = ui_page_setting_common_enter;
    win->poll = NULL;
    win->keyin = ui_page_setting_common_keyin;
}
void ui_page_setting_common_enter(ui_page_context_t* pctx) {
    // Common Settingページの初期化処理
    ui_page_type_t page = pctx->page;
    minyasx_context_t* ctx = pctx->ctx;

    ui_cursor(page, 0, 0);
    ui_print(page, "[Common Setting]\n");
    ui_print(page, ">Speaker   [    ]\n");
    ui_print(page, " AutoDetect[    ]\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, " Reset Defaults\n");
    ui_print(page, "\n");
    ui_print(page, " RETURN");

    // Speakerの現在値を表示
    ui_cursor(page, 11, 1);
    if (ctx->preferences.speaker_enabled) {
        ui_print(page, "[On ]");
    } else {
        ui_print(page, "[Off]");
    }

    // AutoDetectの現在値を表示
    ui_cursor(page, 11, 2);
    switch (ctx->preferences.media_auto_detect) {
    case MEDIA_AUTO_DETECT_DISABLED:
        ui_print(page, "[Off ]");
        break;
    case MEDIA_AUTO_DETECT_60SEC:
        ui_print(page, "[60s ]");
        break;
    case MEDIA_AUTO_DETECT_UNLIMITED:
        ui_print(page, "[Keep]");
        break;
    }
}

//
// Key callback
//
static int position = 1;  // 現在の選択位置 (1-7)

static void set_position(int pos) {
    ui_page_type_t page = UI_PAGE_SETTING_COMMON;
    ui_cursor(page, 0, position);
    ui_print(page, " ");
    int new_pos = pos;
    if (new_pos < 1) new_pos = 1;
    if (new_pos > 7) new_pos = 7;
    // 空行（3, 4, 6行目）を飛ばす
    while ((new_pos == 3) || (new_pos == 4) || (new_pos == 6)) {
        new_pos = (new_pos < position) ? new_pos - 1 : new_pos + 1;
    }
    position = new_pos;
    ui_cursor(page, 0, position);
    ui_print(page, ">");
}

void ui_page_setting_common_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    // Reset Defaultsダイアログが開いている場合
    if (reset_dialog.dialog_open) {
        ui_dialog_keyin(&reset_dialog, keys);
        if (reset_dialog.selection_made) {
            // ダイアログを閉じて背景を復元
            ui_dialog_close(&reset_dialog);
            if (reset_dialog.result) {
                // OKが選択された場合、デフォルトに戻す
                preferences_load_defaults(pctx->ctx);
                preferences_save(pctx->ctx);
                // カーソルを1番に戻してから画面を再描画
                set_position(1);
                ui_page_setting_common_enter(pctx);
            } else {
                set_position(5);  // Cancelの場合はReset Defaultsの位置に戻す
            }
        }
        return;
    }

    // Speakerのプルダウンメニューが開いている場合
    if (!speaker_select.selection_made) {
        ui_select_keyin(&speaker_select, keys);
        if (speaker_select.selection_made) {
            // 選択が確定した
            pctx->ctx->preferences.speaker_enabled = (speaker_select.current_index == 0);
            // 画面を更新
            ui_page_setting_common_enter(pctx);
            set_position(1);  // Speakerの位置に戻す
        }
        return;
    }

    // AutoDetectのプルダウンメニューが開いている場合
    if (!auto_detect_select.selection_made) {
        ui_select_keyin(&auto_detect_select, keys);
        if (auto_detect_select.selection_made) {
            // 選択が確定した
            // 選択されたインデックスを設定に反映
            switch (auto_detect_select.current_index) {
            case 0:
                pctx->ctx->preferences.media_auto_detect = MEDIA_AUTO_DETECT_DISABLED;
                break;
            case 1:
                pctx->ctx->preferences.media_auto_detect = MEDIA_AUTO_DETECT_60SEC;
                break;
            case 2:
                pctx->ctx->preferences.media_auto_detect = MEDIA_AUTO_DETECT_UNLIMITED;
                break;
            }
            // 画面を更新
            ui_page_setting_common_enter(pctx);
            set_position(2);  // AutoDetectの位置に戻す
        }
        return;
    }

    if (keys & UI_KEY_UP) {
        set_position(position - 1);
    }
    if (keys & UI_KEY_DOWN) {
        set_position(position + 1);
    }
    if (keys & UI_KEY_LEFT) {
        // メニューページに戻る
        ui_change_page(UI_PAGE_MENU);
        set_position(1);  // 戻しておく
        return;
    }
    if (keys & UI_KEY_ENTER) {
        switch (position) {
        case 1:
            // Speaker - プルダウンメニューを開く
            speaker_select.current_index = pctx->ctx->preferences.speaker_enabled ? 0 : 1;
            speaker_select.selection_made = false;
            ui_select_init(&speaker_select);
            return;
        case 2:
            // AutoDetect - プルダウンメニューを開く
            auto_detect_select.current_index = pctx->ctx->preferences.media_auto_detect;
            auto_detect_select.selection_made = false;
            ui_select_init(&auto_detect_select);
            return;
        case 5:
            // Reset Defaults - ダイアログを表示
            reset_dialog.selected_button = 1;  // デフォルトはCancel
            reset_dialog.selection_made = false;
            ui_dialog_init(&reset_dialog);
            return;
        case 7:
            // RETURN
            ui_change_page(UI_PAGE_MENU);
            set_position(1);  // 戻しておく
            break;
        default:
            break;
        }
    }
}