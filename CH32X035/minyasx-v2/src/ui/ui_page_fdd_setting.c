#include "ui/ui_control.h"

// FDD Setting page
static void ui_page_setting_fdd_keyin(ui_page_context_t* pctx, ui_key_mask_t keys, int drive);
void ui_page_setting_fdda_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);
void ui_page_setting_fddb_keyin(ui_page_context_t* pctx, ui_key_mask_t keys);
void ui_page_setting_fdd_enter(ui_page_context_t* pctx, int drive);
void ui_page_setting_fdda_enter(ui_page_context_t* pctx);
void ui_page_setting_fddb_enter(ui_page_context_t* pctx);

// RPM選択用UI
static const char* rpm_options[] = {"NONE", "300", "360", "9SCDRV", "BPS", NULL};
ui_select_t rpm_select = {
    .page = UI_PAGE_SETTING_FDDA,  // 初期値、FDD A設定ページ
    .x = 13,
    .y = 1,
    .width = 4,
    .options = rpm_options,
    .option_count = 5,
    .current_index = 0,
    .selection_made = true,  // ここをfalseにすると選択モードに入る
};
// MODE SEL選択用UI
static const char* mode_sel_options[] = {"NORM", "INV", NULL};
ui_select_t mode_sel_select = {
    .page = UI_PAGE_SETTING_FDDA,  // 初期値、FDD A設定ページ
    .x = 13,
    .y = 2,
    .width = 4,
    .options = mode_sel_options,
    .option_count = 2,
    .current_index = 0,
    .selection_made = true,  // ここをfalseにすると選択モードに入る
};
// IN-USE pin選択用UI
static const char* in_use_options[] = {"NONE", "LED", NULL};
ui_select_t in_use_select = {
    .page = UI_PAGE_SETTING_FDDA,  // 初期値、FDD A設定ページ
    .x = 13,
    .y = 3,
    .width = 4,
    .options = in_use_options,
    .option_count = 2,
    .current_index = 0,
    .selection_made = true,  // ここをfalseにすると選択モードに入る
};

void ui_page_setting_fdd_init(ui_page_context_t* win, int drive);

void ui_page_setting_fdda_init(ui_page_context_t* win) {
    win->enter = ui_page_setting_fdda_enter;
    win->poll = NULL;
    win->keyin = ui_page_setting_fdda_keyin;
    ui_page_setting_fdd_init(win, 0);
}
void ui_page_setting_fddb_init(ui_page_context_t* win) {
    win->enter = ui_page_setting_fddb_enter;
    win->poll = NULL;
    win->keyin = ui_page_setting_fddb_keyin;
    ui_page_setting_fdd_init(win, 1);
}

#define DRIVE_LETTER(drive) ((drive) == 0 ? 'A' : 'B')

void ui_page_setting_fdd_init(ui_page_context_t* win, int drive) {
    ui_page_type_t page = win->page;
    ui_cursor(page, 0, 0);
    ui_printf(page, "[FDD %c Setting]\n", DRIVE_LETTER(drive));
    ui_print(page, ">RPM        [----]\n");
    ui_print(page, " MODE SEL   [----]\n");
    ui_print(page, " IN-USE pin [----]\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, "\n");
    ui_print(page, " RETURN");
}

void ui_page_setting_fdda_enter(ui_page_context_t* pctx) {
    ui_page_setting_fdd_enter(pctx, 0);
}

void ui_page_setting_fddb_enter(ui_page_context_t* pctx) {
    ui_page_setting_fdd_enter(pctx, 1);
}

void ui_page_setting_fdd_enter(ui_page_context_t* pctx, int drive) {
    ui_page_type_t page = (drive == 0) ? UI_PAGE_SETTING_FDDA : UI_PAGE_SETTING_FDDB;
    minyasx_context_t* ctx = pctx->ctx;
    // RPM
    ui_cursor(page, 13, 1);
    switch (ctx->drive[drive].rpm_control) {
    case FDD_RPM_CONTROL_NONE:
        ui_print(page, "NONE");
        break;
    case FDD_RPM_CONTROL_300:
        ui_print(page, "300 ");
        break;
    case FDD_RPM_CONTROL_360:
        ui_print(page, "360 ");
        break;
    case FDD_RPM_CONTROL_9SCDRV:
        ui_print(page, "9SCD");
        break;
    case FDD_RPM_CONTROL_BPS:
        ui_print(page, "BPS ");
        break;
    default:
        ui_print(page, "----");
        break;
    }
    ui_cursor(page, 13, 2);
    if (ctx->drive[drive].mode_select_inverted) {
        ui_print(page, "INV ");
    } else {
        ui_print(page, "NORM");
    }
    ui_cursor(page, 13, 3);
    switch (ctx->drive[drive].in_use_mode) {
    case FDD_IN_USE_NONE:
        ui_print(page, "NONE");
        break;
    case FDD_IN_USE_LED:
        ui_print(page, "LED ");
        break;
    default:
        ui_print(page, "----");
        break;
    }
}

//
// Key callback
//
int position = 1;  // 現在の選択位置 (1-7)

static void set_position(int pos, int drive) {
    ui_page_type_t page = (drive == 0) ? UI_PAGE_SETTING_FDDA : UI_PAGE_SETTING_FDDB;
    ui_cursor(page, 0, position);
    ui_print(page, " ");
    int new_pos = pos;
    if (new_pos < 1) new_pos = 1;
    if (new_pos > 7) new_pos = 7;
    while (new_pos == 4 || new_pos == 5 || new_pos == 6) {
        new_pos = (new_pos < position) ? new_pos - 1 : new_pos + 1;  // 空行を飛ばす
    }
    position = new_pos;
    ui_cursor(page, 0, position);
    ui_print(page, ">");
}

void ui_page_setting_fdd_keyin(ui_page_context_t* pctx, ui_key_mask_t keys, int drive);

void ui_page_setting_fdda_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    ui_page_setting_fdd_keyin(pctx, keys, 0);
}
void ui_page_setting_fddb_keyin(ui_page_context_t* pctx, ui_key_mask_t keys) {
    ui_page_setting_fdd_keyin(pctx, keys, 1);
}

void ui_page_setting_fdd_keyin(ui_page_context_t* pctx, ui_key_mask_t keys, int drive) {
    minyasx_context_t* ctx = pctx->ctx;
    // RPM選択モードかどうか
    if (!rpm_select.selection_made) {
        // 選択モード
        ui_select_keyin(&rpm_select, keys);
        if (rpm_select.selection_made) {
            // 選択確定
            ctx->drive[drive].rpm_control = rpm_select.current_index;
            pcfdd_update_setting(ctx, drive);  // 設定変更を反映
        }
        return;  // Enterが押された状態なので一旦 return
    }
    // MODE SEL選択モードかどうか
    if (!mode_sel_select.selection_made) {
        // 選択モード
        ui_select_keyin(&mode_sel_select, keys);
        if (mode_sel_select.selection_made) {
            // 選択確定
            ctx->drive[drive].mode_select_inverted = (mode_sel_select.current_index == 1);
            pcfdd_update_setting(ctx, drive);  // 設定変更を反映
        }
        return;  // Enterが押された状態なので一旦 return
    }
    // IN-USE pin選択モードかどうか
    if (!in_use_select.selection_made) {
        // 選択モード
        ui_select_keyin(&in_use_select, keys);
        if (in_use_select.selection_made) {
            // 選択確定
            ctx->drive[drive].in_use_mode = in_use_select.current_index;
            pcfdd_update_setting(ctx, drive);  // 設定変更を反映
        }
        return;  // Enterが押された状態なので一旦 return
    }
    // 通常モード
    if (keys & UI_KEY_UP) {
        set_position(position - 1, drive);
    }
    if (keys & UI_KEY_DOWN) {
        set_position(position + 1, drive);
    }
    if (keys & UI_KEY_ENTER) {
        switch (position) {
        case 1: {  // RPM
            rpm_select.page = (drive == 0) ? UI_PAGE_SETTING_FDDA : UI_PAGE_SETTING_FDDB;
            rpm_select.current_index = ctx->drive[drive].rpm_control;
            rpm_select.selection_made = false;  // 選択モードに入る
            return;
        }
        case 2: {  // MODE SEL
            mode_sel_select.page = (drive == 0) ? UI_PAGE_SETTING_FDDA : UI_PAGE_SETTING_FDDB;
            mode_sel_select.current_index = ctx->drive[drive].mode_select_inverted ? 1 : 0;
            mode_sel_select.selection_made = false;  // 選択モードに入る
            return;
        }
        case 3: {  // IN-USE pin
            in_use_select.page = (drive == 0) ? UI_PAGE_SETTING_FDDA : UI_PAGE_SETTING_FDDB;
            in_use_select.current_index = ctx->drive[drive].in_use_mode;
            in_use_select.selection_made = false;  // 選択モードに入る
            return;
        }
        case 7: {  // RETURN
            // 戻しておく
            set_position(1, drive);
            ui_change_page(UI_PAGE_MENU);
            break;
        }
        }
    }
}