#ifndef GREENPAK_CONTROL_H
#define GREENPAK_CONTROL_H

#include "i2c/i2c_ch32x035.h"
#include "oled/ssd1306_txt.h"

void gp_reg_set(uint8_t addr7, uint8_t reg, uint8_t val);

void greenpak_dump_oled();

void greenpak_set_dipsw(uint8_t ds0, uint8_t ds1);

// 最終配置（NVMアドレスのみ並べる）
extern const uint8_t gp_target_addr_cleared;
extern const uint8_t gp_target_addr_default;
extern const uint8_t gp_target_addr[4];

#endif  // GREENPAK_CONTROL_H
