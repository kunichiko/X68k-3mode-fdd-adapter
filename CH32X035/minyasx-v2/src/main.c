// ===================================================================================
// Project:  MinyasX V2 with CH32X035
// Author:   Kunihiko Ohnaka (@kunichiko)
// Year:     2025
// URL:      https://kunichiko.ohnaka.jp/
// ===================================================================================

// # プロジェクトの説明
// MinyasXは、X68000用の3モードFDDアダプタです。
// Windows PC用のFDD(3.5インチ、3モード)をX68000に接続して使用するために、
// X68000のFDDにしかない特殊な信号をエミュレーションします。
// また、FDDを動かすために必要な+5V電源、+12V電源をUSB PD(Power Delivery)で供給します。
//
// 詳しい動作は README.md を参照してください。

#include "ch32fun.h"
#include <stdio.h>
#include <string.h>

#include "funconfig.h"

#define WS2812DMA_IMPLEMENTATION

#include "ws2812b_simple.h"

// #define WSRBG //For WS2816C's.
#define WSGRB // For SK6805-EC15
#define NR_LEDS 191

#include "ws2812b_dma_spi_led_driver_alt.h"

#include "color_utilities.h"

uint16_t phases[NR_LEDS];
int frameno;
volatile int tween = -NR_LEDS;

// Callbacks that you must implement.
uint32_t WS2812BLEDCallback(int ledno)
{
	uint8_t index = (phases[ledno]) >> 8;
	uint8_t rsbase = sintable[index];
	uint8_t rs = rsbase >> 3;
	uint32_t fire = ((huetable[(rs + 190) & 0xff] >> 1) << 16) | (huetable[(rs + 30) & 0xff]) | ((huetable[(rs + 0)] >> 1) << 8);
	uint32_t ice = 0x7f0000 | ((rsbase >> 1) << 8) | ((rsbase >> 1));

	// Because this chip doesn't natively support multiplies, we are going to avoid tweening of 1..254.
	return TweenHexColors(fire, ice, ((tween + ledno) > 0) ? 255 : 0); // Where "tween" is a value from 0 ... 255
}

void WS2812_GPIO();
void WS2812_SPI();

int main()
{
	SystemInit();

	// 使用するペリフェラルを有効にする
	// IOPDEN = Port D clock enable
	// IOPCEN = Port C clock enable
	// IOPAEN = Port A clock enable
	// TIM1 = Timer 1 module clock enable
	// AFIO = Alternate Function I/O module clock enable
	RCC->APB2PCENR = RCC_IOPDEN | RCC_IOPCEN | RCC_IOPAEN | RCC_TIM1EN | RCC_AFIOEN;

	// SPIをデフォルトのPA6,7(MISO,MOSI)から、PC6,7(MISO_3,MOSI_3)に変更するために、Remap Register 1 でリマップする
	AFIO->PCFR1 &= ~(AFIO_PCFR1_SPI1_REMAP);						   // SPI1 remap をクリア
	AFIO->PCFR1 |= AFIO_PCFR1_SPI1_REMAP_1 || AFIO_PCFR1_SPI1_REMAP_0; // SPI1 remap を 3 (0b11) にセット

	// Enable GPIOD (for debugging)
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;
	GPIOD->CFGLR &= ~(0xf << (4 * 0));
	GPIOD->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4 * 0);

	GPIOD->BSHR = 1; // Turn on GPIOD0

	WS2812_GPIO();
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

void WS2812_GPIO()
{
	while (1)
	{
		led_data[0][ACCESS_G] = LED_BRIGTHNESS; // Green
		led_data[0][ACCESS_R] = 0;				// Red
		led_data[0][ACCESS_B] = 0;				// Blue
		led_data[0][EJECT_G] = 0;				// Green
		led_data[0][EJECT_R] = LED_BRIGTHNESS;	// Red
		led_data[0][EJECT_B] = 0;				// Blue
		led_data[1][ACCESS_G] = 0;				// Green
		led_data[1][ACCESS_R] = 0;				// Red
		led_data[1][ACCESS_B] = LED_BRIGTHNESS; // Blue
		led_data[1][EJECT_G] = LED_BRIGTHNESS;	// Green
		led_data[1][EJECT_R] = LED_BRIGTHNESS;	// Red
		led_data[1][EJECT_B] = LED_BRIGTHNESS;	// Blue

		// 更新
		WS2812BSimpleSend(GPIOC, 7, (uint8_t *)led_data, 3 * 4);
		Delay_Ms(1000);

		led_data[0][ACCESS_G] = LED_BRIGTHNESS; // Green
		led_data[0][ACCESS_R] = 0;				// Red
		led_data[0][ACCESS_B] = 0;				// Blue
		led_data[0][EJECT_G] = LED_BRIGTHNESS;	// Green
		led_data[0][EJECT_R] = 0;				// Red
		led_data[0][EJECT_B] = 0;				// Blue
		led_data[1][ACCESS_G] = LED_BRIGTHNESS; // Green
		led_data[1][ACCESS_R] = 0;				// Red
		led_data[1][ACCESS_B] = 0;				// Blue
		led_data[1][EJECT_G] = LED_BRIGTHNESS;	// Green
		led_data[1][EJECT_R] = 0;				// Red
		led_data[1][EJECT_B] = 0;				// Blue

		// 更新
		WS2812BSimpleSend(GPIOC, 7, (uint8_t *)led_data, 3 * 4);
		Delay_Ms(1000);
	}
}

void WS2812_SPI()
{
	int k;
	WS2812BDMAInit();

	frameno = 0;

	for (k = 0; k < NR_LEDS; k++)
		phases[k] = k << 8;

	int tweendir = 0;

	while (1)
	{

		GPIOD->BSHR = 1; // Turn on GPIOD0
		// Wait for LEDs to totally finish.
		Delay_Ms(12);
		GPIOD->BSHR = 1 << 16; // Turn it off

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

		for (k = 0; k < NR_LEDS; k++)
		{
			phases[k] += ((((rands[k & 0xff]) + 0xf) << 2) + (((rands[k & 0xff]) + 0xf) << 1)) >> 1;
		}

		WS2812BDMAStart(NR_LEDS);
	}
}