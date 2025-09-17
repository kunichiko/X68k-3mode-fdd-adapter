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

#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "funconfig.h"
#include "greenpak/greenpak_auto.h"
#include "greenpak/greenpak_control.h"
#include "ina3221/ina3221_control.h"
#include "led/led_control.h"
#include "oled/oled_control.h"
#include "pcfdd/pcfdd_control.h"
#include "power_control.h"

void ina3221_poll(uint64_t systick);

int main() {
    SystemInit();
    Delay_Ms(1000);  // Wait for power to stabilize

    RCC->CTLR |= RCC_HSION;  // HSI (48MHz) ON

    // 使用するペリフェラルを有効にする
    // IOPDEN = Port D clock enable
    // IOPCEN = Port C clock enable
    // IOPBEN = Port B clock enable
    // IOPAEN = Port A clock enable
    // TIM1 = Timer 1 module clock enable
    // TIM3 = Timer 3 module clock enable
    // AFIO = Alternate Function I/O module clock enable
    RCC->APB1PCENR |= RCC_TIM3EN;
    RCC->APB2PCENR = RCC_IOPDEN | RCC_IOPCEN | RCC_IOPBEN | RCC_IOPAEN | RCC_TIM1EN | RCC_SPI1EN | RCC_AFIOEN;

    // SPIをデフォルトのPA6,7(MISO,MOSI)から、PC6,7(MISO_3,MOSI_3)に変更するために、Remap Register 1 でリマップする
    AFIO->PCFR1 &= ~(AFIO_PCFR1_SPI1_REMAP);                           // SPI1 remap をクリア
    AFIO->PCFR1 |= AFIO_PCFR1_SPI1_REMAP_1 | AFIO_PCFR1_SPI1_REMAP_0;  // SPI1 remap を 3 (0b11) にセット

    // リセットレジスタのTIM3RSTをセットしてからクリアすると、タイマー3がリセットされる
    RCC->APB1PRSTR |= RCC_APB1Periph_TIM3;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_TIM3;

    // GPIOA
    // PA6 : INDEX_DOSV (入力: INDEX信号, Pull-Up)
    // PA7 : Buzzer
    // PA8 : LED_BLINK (入力: Low=点灯, Pull-Up)
    // PA16: X68_PWR (入力: Low=電源ON要求, Pull-Up)
    // PA17: +12V_EXT_DET (Low=外部+12V電源接続, Pull-Up)
    // PA18: +5V_EN (Low=Enable, High=Disable)
    // PA19: +12V_EN (Low=Enable, High=Disable)
    // PA20: +12V_EXT_EN (Low=Enable, High=Disable))

    // PA6: INDEX_DOSV input
    GPIOA->CFGLR &= ~(0xf << (4 * 6));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 6);
    GPIOA->BSHR = (1 << 6);  // Pull-Up
    // PA7
    GPIOA->CFGLR &= ~(0xf << (4 * 7));
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 7);
    GPIOA->BCR = (1 << 7);  // Low出力にする
    // PA8: LED_BLINK input
    GPIOA->CFGHR &= ~(0xf << (4 * (8 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (8 - 8));
    GPIOA->BSHR = (1 << 8);  // Pull-Up
    // PA16: X68_PWR input
    GPIOA->CFGXR &= ~(0xf << (4 * (16 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (16 - 16));
    GPIOA->BSXR = (1 << (16 - 16));  // Pull-Up
    // PA17: +12V_EXT_DET input
    GPIOA->CFGXR &= ~(0xf << (4 * (17 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (17 - 16));
    GPIOA->BSXR = (1 << (17 - 16));  // Pull-Up
    // PA18: +5V_EN output
    GPIOA->CFGXR &= ~(0xf << (4 * (18 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * (18 - 16));
    GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
    // GPIOA->BSXR = (1 << (18 - 16)); // Enable (+5V_EN=High)
    // PA19: +12V_EN output
    GPIOA->CFGXR &= ~(0xf << (4 * (19 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * (19 - 16));
    GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
    // GPIOA->BSXR = (1 << (19 - 16)); // Enable (+12V_EN=High)
    //  PA20: +12V_EXT_EN output
    GPIOA->CFGXR &= ~(0xf << (4 * (20 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * (20 - 16));
    GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)
    // GPIOA->BSXR = (1 << (20 - 16)); // Enable (+12V_EXT_EN=High)

    // GPIOB
    // PB0 : MODE_SELECT_DOSV (出力: Low=2モード, High=3モード) tbd
    // PB1 : INUSE_DOSV (出力: Low=未使用, High=使用中) tbd
    // PB2 : DRIVE_SEL_DOSV_A (出力: Low=inactive, High=active)
    // PB3 : DRIVE_SEL_DOSV_B (出力: Low=inactive, High=active)
    // PB4 : MOTOR_ON_DOSV (出力: Low=モータOFF, High=モータON)
    // PB5 : DIRECTION_DOSV (出力: Low=非反転, High=反転) tbd
    // PB6 : STEP_DOSV (出力: Low=inactive, High=active)
    // PB7 : SIDE_SELECT_DOSV (出力: Low=表, High=裏)
    // PB8 : DISK_CHANGE_DOSV (入力: Low=ディスクチェンジ, Pull-Up)
    // PB9 : READ_DATA_DOSV (入力: フロッピーディスクの読み出しデータ: Pull-Up)
    // PB10: TRACK0_DOSV (入力: Low=トラック0, Pull-Up)

    // PB0: MODE_SELECT_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 0));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 0);
    GPIOB->BCR = (1 << 0);
    // PB1: INUSE_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 1));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 1);
    GPIOB->BCR = (1 << 1);
    // PB2: DRIVE_SEL_DOSV_A output
    GPIOB->CFGLR &= ~(0xf << (4 * 2));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 2);
    GPIOB->BCR = (1 << 2);
    // PB3: DRIVE_SEL_DOSV_B output
    GPIOB->CFGLR &= ~(0xf << (4 * 3));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 3);
    GPIOB->BCR = (1 << 3);
    // PB4: MOTOR_ON_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 4));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 4);
    // GPIOB->BCR = (1 << 4);
    GPIOB->BSHR = (1 << 4);  // Motor ON for test
    // PB5: DIRECTION_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 5));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 5);
    GPIOB->BCR = (1 << 5);
    // PB6: STEP_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 6));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 6);
    GPIOB->BCR = (1 << 6);
    // PB7: SIDE_SELECT_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 7));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 7);
    GPIOB->BCR = (1 << 7);
    // PB8: DISK_CHANGE_DOSV input
    GPIOB->CFGHR &= ~(0xf << (4 * (8 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (8 - 8));
    GPIOB->BSHR = (1 << 8);  // Pull-Up
    // PB9: READ_DATA_DOSV input
    GPIOB->CFGHR &= ~(0xf << (4 * (9 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (9 - 8));
    GPIOB->BSHR = (1 << 9);  // Pull-Up
    // PB10: TRACK0_DOSV input
    GPIOB->CFGHR &= ~(0xf << (4 * (10 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (10 - 8));
    GPIOB->BSHR = (1 << 10);  // Pull-Up

    // OLEDテスト
    OLED_init();
    OLED_clear();
    OLED_flip(0, 0);
    OLED_print("Hello, OLED!");

    Delay_Ms(2000);

    // INA3221を最初に初期化して電圧電流を測定できるようにする
    ina3221_init();

    // 電源制御を初期化する
    power_control_init();

    // greenpak_force_program_verify(0x02, 2); // GreenPAK3を強制プログラム

    // GreenPAKの自動プログラムと検証
    // greenpak_autoprogram_verify();

    // Delay_Ms(1000);

    // GreenPAKのコンフィグを読み出してOLEDに表示
    // greenpak_dump_oled();

    // LED制御を開始する
    WS2812_SPI_init();

    Delay_Ms(1000);

    //
    pcfdd_init();

    // メインループ
    OLED_clear();
    while (1) {
        uint64_t systick = SysTick->CNT;
        uint32_t ms = systick / (F_CPU / 1000);
        WS2812_SPI_poll();
        ina3221_poll(ms);
        pcfdd_poll(ms);
    }
}
