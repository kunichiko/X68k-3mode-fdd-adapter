#ifndef _WS2812_LED_DRIVER_ALT_H
#define _WS2812_LED_DRIVER_ALT_H

#include "funconfig.h"

#include "ch32fun.h"
#include <stdint.h>

// Use DMA and SPI to stream out WS2812B LED Data via the MOSI pin.
void WS2812BDMAInit();
void WS2812BDMAStart(int leds);

// Callbacks that you must implement.
uint32_t WS2812BLEDCallback(int ledno);

extern volatile int WS2812BLEDInUse;

#endif