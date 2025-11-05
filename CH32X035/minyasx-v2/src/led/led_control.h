#ifndef _LED_CONTROL_H
#define _LED_CONTROL_H

#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "led/color_utilities.h"
#include "led/ws2812b_dma_spi_led_driver_alt.h"
#include "minyasx.h"

typedef enum {
    LED_TEST_MODE_NORMAL = 0,   // 通常動作
    LED_TEST_MODE_ALL_ON = 1,   // 全点灯
    LED_TEST_MODE_ALL_OFF = 2,  // 全消灯
    LED_TEST_MODE_BLINK = 3,    // 点滅
} led_test_mode_t;

void WS2812_SPI_init();
void WS2812_SPI_poll(minyasx_context_t* ctx, uint32_t systick_ms);
void WS2812_SPI_clear();                              // 全LEDを消灯
void WS2812_SPI_set_test_mode(led_test_mode_t mode);  // テストモードを設定
led_test_mode_t WS2812_SPI_get_test_mode();           // テストモード状態を取得

#endif  //_LED_CONTROL_H