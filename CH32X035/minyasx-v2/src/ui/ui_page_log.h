#ifndef UI_PAGE_LOG_H
#define UI_PAGE_LOG_H

#include "ui_control.h"

// UI_LOG���n
void ui_page_log_init(ui_page_context_t* pcon);

// UI_LOG���xn�W�M�ui_writeK�|p��	
void ui_page_log_write_char(char c);

// UI_LOG���nh:��
void ui_page_log_refresh(ui_page_context_t* pctx);

#endif  // UI_PAGE_LOG_H
