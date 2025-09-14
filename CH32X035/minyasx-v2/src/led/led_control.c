#include "led_control.h"

#include "ch32fun.h"
#include <stdio.h>
#include <string.h>

// #define WSRBG //For WS2816C's.
#define WSGRB // For SK6805-EC15
#define NR_LEDS 4

uint16_t phases[NR_LEDS];
int frameno;
volatile int tween = -NR_LEDS;
int tweendir = 0;

// 1/8 にスケール（シフトで軽量）
static inline uint32_t scale_q8(uint32_t c)
{
    uint32_t r = (c >> 16) & 0xFF;
    uint32_t g = (c >> 8) & 0xFF;
    uint32_t b = c & 0xFF;
    r >>= 3;
    g >>= 3;
    b >>= 3;                         // 各8bitを1/8にスケール
    return (r << 16) | (g << 8) | b; // 0xRRGGBB のまま再構成
}

// Callbacks that you must implement.
uint32_t WS2812BLEDCallback(int ledno)
{
    uint8_t index = (phases[ledno]) >> 8;
    uint8_t rsbase = sintable[index];
    uint8_t rs = rsbase >> 3;
    uint32_t fire = ((huetable[(rs + 190) & 0xff] >> 1) << 16) | (huetable[(rs + 30) & 0xff]) | ((huetable[(rs + 0)] >> 1) << 8);
    uint32_t ice = 0x7f0000 | ((rsbase >> 1) << 8) | ((rsbase >> 1));

    // 混色（元の仕様どおり 0 or 255）→ 1/4 にスケールして返す
    // Because this chip doesn't natively support multiplies, we are going to avoid tweening of 1..254.
    uint32_t out = TweenHexColors(fire, ice, ((tween + ledno) > 0) ? 255 : 0); // Where "tween" is a value from 0 ... 255
    return scale_q8(out);
}

// ACCESS A [G,R,B]
// EJECT  A [G,R,B]
// ACCESS B [G,R,B]
// EJECT  B [G,R,B]
uint8_t led_data[2][3 * 2] = {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
#define ACCESS_G 0
#define ACCESS_R 1
#define ACCESS_B 2
#define EJECT_G 3
#define EJECT_R 4
#define EJECT_B 5
#define LED_BRIGTHNESS 10

void WS2812_SPI_init()
{
    int k;
    WS2812BDMAInit();

    frameno = 0;

    for (k = 0; k < NR_LEDS; k++)
        phases[k] = k << 8;
}

void WS2812_SPI_poll()
{
    GPIOA->BSHR = 1 << 7; // Turn on PA7
    // Wait for LEDs to totally finish.
    Delay_Ms(12);
    GPIOA->BSHR = 1 << (7 + 16); // Turn it off

    while (WS2812BLEDInUse)
        ;

    frameno++;

    if (frameno == 1024)
    {
        tweendir = 1;
    }
    if (frameno == 2048)
    {
        tweendir = -1;
        frameno = 0;
    }

    if (tweendir)
    {
        int t = tween + tweendir;
        if (t > 255)
            t = 255;
        if (t < -NR_LEDS)
            t = -NR_LEDS;
        tween = t;
    }

    for (int k = 0; k < NR_LEDS; k++)
    {
        phases[k] += ((((rands[k & 0xff]) + 0xf) << 2) + (((rands[k & 0xff]) + 0xf) << 1)) >> 1;
    }

    WS2812BDMAStart(NR_LEDS);
}