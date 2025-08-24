#include "main.h"

#include <stdarg.h>
#include <stdio.h>

#include "ch32v003_GPIO_branchless.h"
#include "ch32v003fun.h"

#include <ssd1306_txt.h>        // OLED text functions

#include "port_polling.h"

#define WS2812BSIMPLE_IMPLEMENTATION
#include "ws2812b_simple.h"

#define MAIN_LOOP_INTERVAL 100  // 100msごとにメインループを回す

void update_fdd_led();

/**
 * @brief 設定ピンによって、MODE_SELECTの論理を反転するかどうかのフラグ
 *
 */
bool mode_select_invert = 0;

int main() {
    SystemInit();

    Delay_Ms(100);
  
    // Setup OLED
    OLED_init();
    OLED_clear();
    OLED_printf("Minyax-X");

    debugprint("UP\n");

    // 使用するペリフェラルを有効にする
    // IOPDEN = Port D clock enable
    // IOPCEN = Port C clock enable
    // IOPAEN = Port A clock enable
    // TIM1 = Timer 1 module clock enable
    // AFIO = Alternate Function I/O module clock enable
    RCC->APB2PCENR = RCC_IOPDEN | RCC_IOPCEN | RCC_IOPAEN | RCC_TIM1EN | RCC_AFIOEN;

    // リセットレジスタのTIM1RSTをセットしてからクリアすると、
    // タイマー1がリセットされる
    RCC->APB2PRSTR = RCC_TIM1RST;
    RCC->APB2PRSTR = 0;

    //
    // GPIOの設定
    //

    // GPIO A
    // A1: READY,                   Output, Push-Pull
    // A2: BUZZER,                  Output, Push-Pull
    GPIOA->CFGLR &= ~(0xf << (4 * 1));
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 1);
    GPIOA->CFGLR &= ~(0xf << (4 * 2));
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 2);

    // GPIO C
    // C0: LED_DATA, Output, push-pull
    //     C0にはスイッチがついていて起動時にHighかLowかでMODE_SELECTの論理を決める
    // C1: I2C SDA
    // C2: I2C SCL
    // C3: FDD INDEX,               Input, pull-up
    // C4: FDD MOTOR_ON,            Input, pull-up
    // C5: FDD DRIVE_SELECT,        Input, pull-up
    // C6: FDD OPTION_SELECT_PAIR,  Input, pull-up
    // C7: FDD OPTION_SELECT,       Input, pull-up

    GPIOC->CFGLR &= ~(0xf << (4 * 0));
    GPIOC->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 0);
    GPIOC->BSHR = GPIO_Pin_0;  // プルアップを有効にする
    if ((GPIOC->INDR & GPIO_Pin_0) == 0) {
        mode_select_invert = 1;
    } else {
        mode_select_invert = 0;
    }
    debugprint("MODE_SELECT_INVERT: %d\n", mode_select_invert);

    // C0を出力にする
    GPIOC->CFGLR &= ~(0xf << (4 * 0));
    GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 0);

    GPIOC->CFGLR &= ~(0xf << (4 * 3));
    GPIOC->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 3);
    GPIOC->CFGLR &= ~(0xf << (4 * 4));
    GPIOC->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 4);
    GPIOC->CFGLR &= ~(0xf << (4 * 5));
    GPIOC->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 5);
    GPIOC->CFGLR &= ~(0xf << (4 * 6));
    GPIOC->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 6);
    GPIOC->CFGLR &= ~(0xf << (4 * 7));
    GPIOC->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 7);

    // PUPDモードの入力ピンに1を出力すると、プルアップになる
    // C3-C7をプルアップにする
    GPIOC->BSHR = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;

    // GPIO D
    // D0: DISK_IN_GEN,         Output, push-pull
    // D1: DISK_CHANGE_DOSV,    Input, pull-up
    // D2: DRIVE_SELECT_DOSV_1, Output, Open-drain
    // D3: MODE_SELECT_DOSV,    Output, Open-drain
    // D4: EJECT_MASK,          Input, pull-up
    // D5: EJECT,               Input, pull-up
    // D6: LED_BLINK,           Input, pull-up
    // D7: FDD_INT_GEN,         Output, push-pull
    GPIOD->CFGLR &= ~(0xf << (4 * 0));
    GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 0);
    GPIOD->CFGLR &= ~(0xf << (4 * 1));
    GPIOD->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 1);
    GPIOD->CFGLR &= ~(0xf << (4 * 2));
    GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_OD) << (4 * 2);
    GPIOD->CFGLR &= ~(0xf << (4 * 3));
    GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_OD) << (4 * 3);
    GPIOD->CFGLR &= ~(0xf << (4 * 4));
    GPIOD->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 4);
    GPIOD->CFGLR &= ~(0xf << (4 * 5));
    GPIOD->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 5);
    GPIOD->CFGLR &= ~(0xf << (4 * 6));
    GPIOD->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 6);
    GPIOD->CFGLR &= ~(0xf << (4 * 7));
    GPIOD->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 7);

    // PUPDモードの入力ピンに1を出力すると、プルアップになる
    // D1, D4, D5, D6をプルアップにする
    GPIOD->BSHR = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;

    // MODE_SELECTの論理の初期設定
    // 初期状態はX68000の標準フォーマット(360RPM, 1.2MB)とする
    if (mode_select_invert) {
        // 反転時はMODE_SELECT_DOSV(D3)をアクティブ(Low)にすると1.2MB
        GPIOD->BCR = GPIO_Pin_3;
    } else {
        // 通常時はMODE_SELECT_DOSV(D3)をインアクティブ(High)にすると1.2MB
        GPIOD->BSHR = GPIO_Pin_3;
    }

    // Timer 1 を 10us で割り込みを発生させる
    // 10us = 100kHz で割り込みを発生させる

    port_polling_init();

    while (1) {
        Delay_Ms(MAIN_LOOP_INTERVAL);
        update_fdd_led();
    }
}

/*
 X68000の純正ドライブのLEDの仕様は以下の通り

* アクセスランプ
    * メディア挿入時
        * アクセスなし            緑点灯
        * アクセスあり            赤点灯
    * メディア未挿入時
        * LED_BLINK = false     消灯
        * LED_BLINK = true      緑点滅
* イジェクトランプ
    * メディア挿入時
        * EJECT_MASK = false     緑点灯 (マスクがfalseなので、イジェクト可能)
        * EJECT_MASK = true      消灯   (マスクがtrueなので、イジェクト不可)
    * メディア未挿入時
        * 常時                   消灯

 ※メディアの挿入/未挿入の判断、アクセスの有無はドライブ自体が行なっている。

 上記を踏まえ、本装置では以下のように実装する。

* メディア挿入/未挿入の判断
    * READY信号がアクティブになったら挿入状態に移行
    * DISK_CHANGEがアクティブになったら未挿入状態に移行
* アクセスあり/なしの判断
    * DRIVE_SELECTがアクティブになっている間はアクセスあり
* LED_BLINK
    * DRIVE_SELECTアサート時の値を保持
* EJECT_MASK
    * DRIVE_SELECTアサート時の値を保持
 */

volatile bool in_access = 0;
volatile bool media_inserted = 0;
volatile bool led_blink = 0;
volatile bool eject = 0;
volatile bool eject_mask = 0;

int led_blink_counter = 0;

// ACCESS [G,R,B]
// EJECT  [G,R,B]
uint8_t led_data[3 * 2] = {0, 0, 0, 0, 0, 0};
#define ACCESS_G 0
#define ACCESS_R 1
#define ACCESS_B 2
#define EJECT_G 3
#define EJECT_R 4
#define EJECT_B 5
#define LED_BLINK_INTERVAL 1000 * 2
#define LED_BRIGTHNESS 10

void update_fdd_led() {
    // 一旦データをクリア
    for (int i = 0; i < 3; i++) {
        led_data[i] = 0;
        led_data[i + 3] = 0;
    }
    // LED_BLINK カウンターの更新
    led_blink_counter += MAIN_LOOP_INTERVAL;
    if (led_blink_counter >= LED_BLINK_INTERVAL) {
        led_blink_counter = 0;
    }
    int led_blink_brightness = led_blink_counter < LED_BLINK_INTERVAL / 2 ? LED_BRIGTHNESS : 0;

    // アクセスランプの判断
    if (media_inserted) {
        // メディア挿入
        if (in_access) {
            // アクセスあり = 赤点灯
            led_data[ACCESS_R] = LED_BRIGTHNESS;
        } else {
            // アクセスなし = 緑点灯
            led_data[ACCESS_G] = LED_BRIGTHNESS;
        }
    } else {
        // メディア未挿入の場合は LED_BLINK によって点滅か点灯
        if (led_blink) {
            led_data[ACCESS_G] = led_blink_brightness;
        } else {
            led_data[ACCESS_G] = 0;
        }
    }
    // イジェクトランプの判断
    if (media_inserted) {
        // メディア挿入
        if (eject_mask) {
            // イジェクト不可 = 消灯
            led_data[EJECT_G] = 0;
        } else {
            // イジェクト可能 = 緑点灯
            if (revolution_2hd_144) {
                led_data[EJECT_B] = LED_BRIGTHNESS;  // 1.44MBの場合は青点灯
            } else {
                led_data[EJECT_G] = LED_BRIGTHNESS;
            }
        }
    } else {
        // メディア未挿入の場合は消灯
        led_data[EJECT_G] = 0;
    }

    // PD1 (SWIOとの共用ピン) が読めるかどうかのテスト
    // if(GPIOD->INDR & GPIO_Pin_1) {
    //     led_data[EJECT_B] = LED_BRIGTHNESS;
    // } else {
    //     led_data[EJECT_R] = 0;
    // }

    // 更新
    WS2812BSimpleSend(GPIOC, 0, led_data, 6);
}