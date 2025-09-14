#ifndef _LED_CONTROL_H
#define _LED_CONTROL_H

#include "ch32fun.h"
#include <stdio.h>
#include <string.h>

#include "led/ws2812b_dma_spi_led_driver_alt.h"
#include "led/color_utilities.h"

void WS2812_SPI();

#endif //_LED_CONTROL_H