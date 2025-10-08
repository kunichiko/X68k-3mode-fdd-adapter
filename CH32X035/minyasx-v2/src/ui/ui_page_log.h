#ifndef UI_PAGE_LOG_H
#define UI_PAGE_LOG_H

#include "ui_control.h"

// UI_LOGÚü¸n
void ui_page_log_init(ui_page_context_t* pcon);

// UI_LOGÚü¸xn‡WøM¼ui_writeK‰|pŒ‹	
void ui_page_log_write_char(char c);

// UI_LOGÚü¸nh:ô°
void ui_page_log_refresh(ui_page_context_t* pctx);

#endif  // UI_PAGE_LOG_H
