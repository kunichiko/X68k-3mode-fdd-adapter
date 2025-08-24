#pragma once

#include "ch32v003fun.h"

void I2C_start(uint8_t addr);
void I2C_init(void);
void I2C_write(uint8_t data);

#define DLY_ms(n)         Delay_Ms(n)  // delay n milliseconds
