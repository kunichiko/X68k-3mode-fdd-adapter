#ifndef GREENPAK_CONTROL_H
#define GREENPAK_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c/i2c_ch32x035.h"
#include "oled/ssd1306_txt.h"

#define GP_NUM 5  // GreenPAKの数

uint8_t gp_reg_get(uint8_t addr7, uint8_t reg);

void gp_reg_set(uint8_t addr7, uint8_t reg, uint8_t val);

void greenpak_dump_oled();

uint8_t greenpak_get_virtualinput(int unit);

void greenpak_set_virtualinput(int unit, uint8_t val);

bool greenpak_get_matrixinput(int unit, uint8_t inputno);

// 最終配置（NVMアドレスのみ並べる）
extern const uint8_t gp_target_addr_cleared;
extern const uint8_t gp_target_addr_default;
extern const uint8_t gp_target_addr[GP_NUM];

#endif  // GREENPAK_CONTROL_H
