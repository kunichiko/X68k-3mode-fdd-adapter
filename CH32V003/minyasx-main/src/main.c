#include <stdarg.h>
#include <stdio.h>

#include "ch32v003_GPIO_branchless.h"
#include "ch32v003fun.h"

#include "main.h"
#include "port_polling.h"

#define WS2812BSIMPLE_IMPLEMENTATION

#include "ws2812b_simple.h"


int main() {
    SystemInit();

    Delay_Ms(100);

    debugprint("UP\n");

    // 使用するペリフェラルを有効にする
    // IOPDEN = Port D clock enable
    // IOPCEN = Port C clock enable
    // IOPAEN = Port A clock enable
    // TIM1 = Timer 1 module clock enable
    // AFIO = Alternate Function I/O module clock enable
    RCC->APB2PCENR =
        RCC_IOPDEN | RCC_IOPCEN | RCC_IOPAEN | RCC_TIM1EN | RCC_AFIOEN;

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
    // C1: I2C SDA
    // C2: I2C SCL
    // C3: FDD INDEX,               Input, pull-up 
    // C4: FDD MOTOR_ON,            Input, pull-up
    // C5: FDD DRIVE_SELECT,        Input, pull-up
    // C6: FDD OPTION_SELECT_PAIR,  Input, pull-up
    // C7: FDD OPTION_SELECT,       Input, pull-up
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
    

    // Timer 1 を 10us で割り込みを発生させる
    // 10us = 100kHz で割り込みを発生させる


    port_polling_init();

    // ACCESS [G,R,B]
    // EJECT  [G,R,B]
    uint8_t data[3*2] = {10, 0, 0, 0, 10, 0};
    WS2812BSimpleSend(GPIOC, 0, data, 6);

    while(1) {
        Delay_Ms(1000);
        data[0] = 0;
        WS2812BSimpleSend(GPIOC, 0, data, 6);
        Delay_Ms(1000);
        data[0] = 10;
        WS2812BSimpleSend(GPIOC, 0, data, 6);
    }
}