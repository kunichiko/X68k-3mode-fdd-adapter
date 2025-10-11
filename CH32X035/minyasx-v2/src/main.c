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
#include "power/power_control.h"
#include "preferences/preferences_control.h"
#include "sound/play_control.h"
#include "ui/ui_control.h"
#include "x68fdd/x68fdd_control.h"

int main() {
    SystemInit();

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
    // PA0 : DRIVE_SELECT_A (入力: Low=active, High=inactive, Pull-Up)
    // PA1 : DRIVE_SELECT_B (入力: Low=active, High=inactive, Pull-Up)
    // PA2 : OPTION_SELECT_A (入力: Low=active, High=inactive, Pull-Up)
    // PA3 : OPTION_SELECT_B (入力: Low=active, High=inactive, Pull-Up)
    // PA4 : EJECT (入力: Low=イジェクト, Pull-Up)
    // PA5 : EJECT_MASK (入力: Low=イジェクト禁止, Pull-Up)
    // PA6 : INDEX_DOSV (入力: INDEX信号, Pull-Up)
    // PA7 : Buzzer (出力: PWM)
    // PA8 : LED_BLINK (入力: Low=点灯, Pull-Up)
    // PA9 : DISK_TYPE_SELECT (入力: Pull-Up, 未使用)
    // PA10: I2C_SCL (I2Cクロック)
    // PA11: I2C_SDA (I2Cデータ)
    // PA12: MOTOR_ON (入力: Low=ON, High=OFF, Pull-Up)
    // PA13: DIRECTION (入力: Low=正転, High=逆転, Pull-Up) ?
    // PA14: STEP (入力: Low→Highで1ステップ, Pull-Up) ?
    // PA15: SIDE_SELECT (入力: Low=SIDE0, High=SIDE1, Pull-Up) ?
    // PA16: X68_PWR (入力: Low=電源ON要求, Pull-Up)
    // PA17: +12V_EXT_DET (High=外部+12V電源接続, Pull-Up)
    // PA18: +5V_EN (Low=Enable, High=Disable)
    // PA19: +12V_EN (Low=Enable, High=Disable)
    // PA20: +12V_EXT_EN (Low=Enable, High=Disable))

    // PA0: DRIVE_SELECT_A input
    GPIOA->CFGLR &= ~(0xf << (4 * 0));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 0);
    GPIOA->BSHR = (1 << 0);  // Pull-Up
    // PA1: DRIVE_SELECT_B input
    GPIOA->CFGLR &= ~(0xf << (4 * 1));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 1);
    GPIOA->BSHR = (1 << 1);  // Pull-Up
    // PA2: OPTION_SELECT_A input
    GPIOA->CFGLR &= ~(0xf << (4 * 2));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 2);
    GPIOA->BSHR = (1 << 2);  // Pull-Up
    // PA3: OPTION_SELECT_B input
    GPIOA->CFGLR &= ~(0xf << (4 * 3));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 3);
    GPIOA->BSHR = (1 << 3);  // Pull-Up
    // PA4: EJECT input
    GPIOA->CFGLR &= ~(0xf << (4 * 4));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 4);
    GPIOA->BSHR = (1 << 4);  // Pull-Up
    // PA5: EJECT_MASK input
    GPIOA->CFGLR &= ~(0xf << (4 * 5));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 5);
    GPIOA->BSHR = (1 << 5);  // Pull-Up
    // PA6: INDEX_DOSV input
    GPIOA->CFGLR &= ~(0xf << (4 * 6));
    GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 6);
    GPIOA->BSHR = (1 << 6);  // Pull-Up
    // PA7 : Buzzer output. TIM3_CH2 (PWM出力)
    GPIOA->CFGLR &= ~(0xf << (4 * 7));
    GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 7);
    // GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 7);
    //  GPIOA->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 7);
    GPIOA->BCR = (1 << 7);  // Low出力にする
    // GPIOA->BSHR = (1 << 7);  // High出力にする (Buzzer OFF)
    //  PA8: LED_BLINK input
    GPIOA->CFGHR &= ~(0xf << (4 * (8 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (8 - 8));
    GPIOA->BSHR = (1 << 8);  // Pull-Up
    // PA9: DISK_TYPE_SELECT input
    GPIOA->CFGHR &= ~(0xf << (4 * (9 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (9 - 8));
    GPIOA->BSHR = (1 << 9);  // Pull-Up
    // PA10: I2C_SCL
    // PA11: I2C_SDA
    // PA12: MOTOR_ON input
    GPIOA->CFGHR &= ~(0xf << (4 * (12 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (12 - 8));
    GPIOA->BSHR = (1 << 12);  // Pull-Up
    // PA13: DIRECTION input
    GPIOA->CFGHR &= ~(0xf << (4 * (13 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (13 - 8));
    GPIOA->BSHR = (1 << 13);  // Pull-Up
    // PA14: STEP input
    GPIOA->CFGHR &= ~(0xf << (4 * (14 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (14 - 8));
    GPIOA->BSHR = (1 << 14);  // Pull-Up
    // PA15: SIDE_SELECT input
    GPIOA->CFGHR &= ~(0xf << (4 * (15 - 8)));
    GPIOA->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (15 - 8));
    GPIOA->BSHR = (1 << 15);  // Pull-Up
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
    // PB0 : MODE_SELECT_DOSV (出力: Low=360RPM, High=300RPM, 逆のものもあるらしい)
    // PB1 : IN_USE_MCU (出力: Low=アクティブ, High=非アクティブ)
    // PB2 : DRIVE_SEL_DOSV_A (未使用)
    // PB3 : DRIVE_SEL_DOSV_B (未使用)
    // PB4 : MOTOR_ON_DOSV (未使用)
    // PB5 : DIRECTION_DOSV (未使用)
    // PB6 : STEP_DOSV (未使用)
    // PB7 : SIDE_SELECT_DOSV (未使用)→　この端子はLOCK_ACKに変更しました
    // PB8 : DISK_CHANGE_DOSV (入力: Low=ディスクチェンジ, Pull-Up)
    // PB9 : READ_DATA_DOSV (入力: フロッピーディスクの読み出しデータ: Pull-Up)
    // PB10: TRACK0_DOSV (入力: Low=トラック0, Pull-Up)
    // PB11: OPTION_SELECT_B_PAIR (入力: Low=active, High=inactive, Pull-Up)
    // PB12: READY_MCU_A (出力: Low=準備完了, High=準備完了でない)
    // PB13: READY_MCU_B (出力: Low=準備完了
    //
    // PA22: DIPSW_DS0 (入力: Pull-Up)
    // PA23: DIPSW_DS1 (入力: Pull-Up)

    // PB0: MODE_SELECT_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 0));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 0);
    GPIOB->BCR = (1 << 0);
    // PB1: IN_USE_MCU output
    GPIOB->CFGLR &= ~(0xf << (4 * 1));
    GPIOB->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 1);
    GPIOB->BCR = (1 << 1);
    // PB2: DRIVE_SELECT_DOSV_A output (未使用)
    GPIOB->CFGLR &= ~(0xf << (4 * 2));
    GPIOB->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << (4 * 2);
    // PB3: DRIVE_SELECT_DOSV_B output (未使用)
    GPIOB->CFGLR &= ~(0xf << (4 * 3));
    GPIOB->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << (4 * 3);
    // PB4: MOTOR_ON_DOSV output
    GPIOB->CFGLR &= ~(0xf << (4 * 4));
    GPIOB->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << (4 * 4);
    GPIOB->BCR = (1 << 4);
    // PB5: DIRECTION_DOSV (Not-Used)
    GPIOB->CFGLR &= ~(0xf << (4 * 5));
    GPIOB->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << (4 * 5);
    // GPIOB->BCR = (1 << 5);
    //  PB6: STEP_DOSV (Not-Used)
    GPIOB->CFGLR &= ~(0xf << (4 * 6));
    GPIOB->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << (4 * 6);
    //    GPIOB->BCR = (1 << 6);
    // PB7: LOCK_ACK input
    GPIOB->CFGLR &= ~(0xf << (4 * 7));
    GPIOB->CFGLR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * 7);
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
    // PB11: OPTION_SELECT_B_PAIR input
    GPIOB->CFGHR &= ~(0xf << (4 * (11 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (11 - 8));
    GPIOB->BSHR = (1 << 11);  // Pull-Up
    // PB12: READY_MCU_A output
    GPIOB->CFGHR &= ~(0xf << (4 * (12 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * (12 - 8));
    GPIOB->BSHR = (1 << 12);
    // PB13: READY_MCU_B output
    GPIOB->CFGHR &= ~(0xf << (4 * (13 - 8)));
    GPIOB->CFGHR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * (13 - 8));
    GPIOB->BSHR = (1 << 13);

    // PA22: DIPSW_DS0 input
    GPIOA->CFGXR &= ~(0xf << (4 * (22 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (22 - 16));
    GPIOA->BSXR = (1 << (22 - 16));  // Pull-Up
    // PA23: DIPSW_DS1 input
    GPIOA->CFGXR &= ~(0xf << (4 * (23 - 16)));
    GPIOA->CFGXR |= (GPIO_Speed_In | GPIO_CNF_IN_PUPD) << (4 * (23 - 16));
    GPIOA->BSXR = (1 << (23 - 16));  // Pull-Up

    // GPIOC
    // PC6 : LOCK_REQ (出力: High=LocRequest, Low=NotRequest)
    GPIOC->CFGLR &= ~(0xf << (4 * 6));
    GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 6);
    GPIOC->BCR = (1 << 6);  // Disable (Low)

    //
    // コンテキストの初期化
    //
    minyasx_context_t* ctx = minyasx_init();

    // UIシステムを初期化する
    ui_init(ctx);
    ui_change_page(UI_PAGE_BOOT);

    Delay_Ms(1000);
    ui_change_page(UI_PAGE_LOG);

    // Preferencesを初期化する
    preferences_init(ctx);

    // INA3221を初期化して電圧電流を測定できるようにする
    ina3221_init();

    //
    // greenpak_force_program_verify(0x2a, 4);  // GreenPAKを強制プログラム

    // GreenPAKのコンフィグを読み出してOLEDに表示
    // greenpak_dump_oled();

    // GreenPAKの自動プログラムと検証
    greenpak_autoprogram_verify();

    // 電源制御を初期化する
    power_control_init(ctx);

    // PVD（電源電圧検出）を初期化する
    power_pvd_init();

    // Delay_Ms(1000);

    // LED制御を開始する
    ui_log(UI_LOG_LEVEL_INFO, "LED Init\n");
    WS2812_SPI_init();

    Delay_Ms(500);

    //
    ui_log(UI_LOG_LEVEL_INFO, "PCFDD Init\n");
    pcfdd_init(ctx);
    ui_log(UI_LOG_LEVEL_INFO, "X68kFDD IF Init\n");
    x68fdd_init(ctx);

    // FDD IDの設定
    ui_log(UI_LOG_LEVEL_INFO, "Setting FDD ID\n");
    x68fdd_update_drive_id(ctx);

    // 以下もセットする
    // GP2の DISK_IN_A_n (Virtual Input 2=bit5)
    // GP2の DISK_IN_B_n (Virtual Input 3=bit4)
    // GP2の ERR_DISK_A_n (Virtual Input 4=bit3)
    // GP2の ERR_DISK_B_n (Virtual Input 5=bit2)
    // GP3の DISK_IN_A_n (Virtual Input 2=bit5)
    // GP3の DISK_IN_B_n (Virtual Input 3=bit4)
    uint8_t gp2_vin = greenpak_get_virtualinput(2 - 1);
    gp2_vin &= 0xc0;  // 仮に両方ともディスクあり
    gp2_vin |= 0x0c;  // エラーなしにする
    greenpak_set_virtualinput(2 - 1, gp2_vin);
    uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
    gp3_vin &= 0xc0;  // 仮に両方ともディスクありにする
    greenpak_set_virtualinput(3 - 1, gp3_vin);

    // 音再生コンテキストの初期化
    // タイマーの初期化の関係があるので pcfdd_init() の後に呼ぶ
    ui_log(UI_LOG_LEVEL_INFO, "Play Init\n");
    play_init(ctx);

    //
    // メインループ
    //
    ui_clear(UI_PAGE_DEBUG);
    ui_change_page(UI_PAGE_MAIN);

    // 音再生テスト
    // play_start_melody(ctx, &melody_power_on);

    // ui_log_set_level(UI_LOG_LEVEL_INFO);
    ui_log_set_level(UI_LOG_LEVEL_DEBUG);
    // ui_log_set_level(UI_LOG_LEVEL_TRACE);

    // test
    // ctx->drive[1].mode_select_inverted = true;

    // メインループに入る前に、X68000の電源状態を初期チェック
    uint64_t systick = SysTick->CNT;
    uint32_t ms = systick / (F_CPU / 1000);
    power_control_poll(ctx, ms);

    while (1) {
        uint64_t systick = SysTick->CNT;
        uint32_t ms = systick / (F_CPU / 1000);
        power_control_poll(ctx, ms);

        // 電源状態に基づいた画面遷移を一元管理
        if (ctx->power_on) {
            // 電源ONの場合、BOOT画面ならMAIN画面に遷移
            if (ui_get_current_page() == UI_PAGE_BOOT) {
                ui_change_page(UI_PAGE_MAIN);
            }
        } else {
            // 電源OFFの場合、BOOT画面でなければBOOT画面に遷移
            if (ui_get_current_page() != UI_PAGE_BOOT) {
                WS2812_SPI_clear();  // 全LEDを消灯
                ui_change_page(UI_PAGE_BOOT);
            }
        }

        ui_poll(ctx, ms);

        if (ctx->power_on) {
            ui_log(UI_LOG_LEVEL_TRACE, "1");
            WS2812_SPI_poll(ctx, ms);
            ui_log(UI_LOG_LEVEL_TRACE, "2");
            ina3221_poll(ctx, ms);
            ui_log(UI_LOG_LEVEL_TRACE, "3");
            pcfdd_poll(ctx, ms);
            ui_log(UI_LOG_LEVEL_TRACE, "4");
            x68fdd_poll(ctx, ms);
            ui_log(UI_LOG_LEVEL_TRACE, "6");
            play_poll(ctx, ms);
            ui_log(UI_LOG_LEVEL_TRACE, "7");
            preferences_poll(ctx, ms);
            ui_log(UI_LOG_LEVEL_TRACE, "8");
        }
    }
}
