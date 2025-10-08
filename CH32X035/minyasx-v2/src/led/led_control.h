#ifndef _LED_CONTROL_H
#define _LED_CONTROL_H

#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "led/color_utilities.h"
#include "led/ws2812b_dma_spi_led_driver_alt.h"
#include "minyasx.h"

void WS2812_SPI_init();
void WS2812_SPI_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void WS2812_SPI_clear();  // 全LEDを消灯

#endif  //_LED_CONTROL_H