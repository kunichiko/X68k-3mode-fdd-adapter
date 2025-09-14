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

#include "led/led_control.h"

int main()
{
	SystemInit();

	// 使用するペリフェラルを有効にする
	// IOPDEN = Port D clock enable
	// IOPCEN = Port C clock enable
	// IOPAEN = Port A clock enable
	// TIM1 = Timer 1 module clock enable
	// AFIO = Alternate Function I/O module clock enable
	RCC->APB2PCENR = RCC_IOPDEN | RCC_IOPCEN | RCC_IOPAEN | RCC_TIM1EN | RCC_SPI1EN | RCC_AFIOEN;

	// SPIをデフォルトのPA6,7(MISO,MOSI)から、PC6,7(MISO_3,MOSI_3)に変更するために、Remap Register 1 でリマップする
	AFIO->PCFR1 &= ~(AFIO_PCFR1_SPI1_REMAP);						  // SPI1 remap をクリア
	AFIO->PCFR1 |= AFIO_PCFR1_SPI1_REMAP_1 | AFIO_PCFR1_SPI1_REMAP_0; // SPI1 remap を 3 (0b11) にセット

	// GPIOA
	// PA7 = Buzzer
	GPIOA->CFGLR &= ~(0xf << (4 * 7));
	GPIOA->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4 * 7);

	GPIOA->BSHR = 1 << 7; // Turn on PA7

	WS2812_SPI();
}
