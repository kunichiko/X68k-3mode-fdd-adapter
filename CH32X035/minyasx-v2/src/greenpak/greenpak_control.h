#ifndef GREENPAK_CONTROL_H
#define GREENPAK_CONTROL_H

#include "i2c/i2c_ch32x035.h"
#include "oled/ssd1306_txt.h"

void greenpak_dump_oled();

// 最終配置（NVMアドレスのみ並べる）
extern const uint8_t gp_target_nvm[4];

#endif  // GREENPAK_CONTROL_H
