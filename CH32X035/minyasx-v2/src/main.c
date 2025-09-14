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

#include "oled/oled_control.h"
#include "led/led_control.h"
#include "greenpak/greenpak_auto.h"
#include "greenpak/greenpak_control.h"
#include "ina3221/ina3221_control.h"

void ina3221_poll(uint64_t systick);

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

	// OLEDテスト
	OLED_init();
	OLED_clear();
	OLED_flip(0, 0);
	OLED_print("Hello, OLED!");

	Delay_Ms(3000);

	// INA3221
	ina3221_init();

	// greenpak_force_program_verify(0x02, 2); // GreenPAK3を強制プログラム

	// GreenPAKの自動プログラムと検証
	greenpak_autoprogram_verify();

	Delay_Ms(1000);

	// GreenPAKのコンフィグを読み出してOLEDに表示
	greenpak_dump_oled();

	// LED制御を開始する
	WS2812_SPI_init();

	while (1)
	{
		uint64_t systick = SysTick->CNT;
		uint32_t ms = systick / (F_CPU / 1000);
		WS2812_SPI_poll();
		ina3221_poll(ms);
	}
}

void ina3221_poll(uint64_t systick_ms)
{
	static uint64_t last_tick = 0;
	if (systick_ms - last_tick < 1000)
	{
		return;
	}
	last_tick = systick_ms;
	uint16_t ch1_current, ch1_voltage, ch2_current, ch2_voltage, ch3_current, ch3_voltage;

	ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);

	OLED_clear();
	OLED_write('\n');
	OLED_printf("VBUS:%2d.%02dV %4dmA", ch1_voltage / 1000, (ch1_voltage % 1000) / 10, ch1_current);
	OLED_write('\n');
	OLED_printf("+12V:%2d.%02dV %4dmA", ch2_voltage / 1000, (ch2_voltage % 1000) / 10, ch2_current);
	OLED_write('\n');
	OLED_printf("+5V :%2d.%02dV %4dmA", ch3_voltage / 1000, (ch3_voltage % 1000) / 10, ch3_current);
	OLED_write('\n');
}