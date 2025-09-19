#include "x68fdd/x68fdd_control.h"

#include "oled/ssd1306_txt.h"

volatile uint32_t exti_int_counter = 0;

void x68fdd_init(void) {
    // X68000側からのアクセスに割り込みで応答するために、以下のGPIOの割り込みを設定する
    //
    // PA0 : DRIVE_SELECT_A
    // PA1 : DRIVE_SELECT_B
    // PA2 : OPTION_SELECT_A
    // PA3 : OPTION_SELECT_B
    // PA12: MOTOR_ON
    // PA13: DIRECTION
    // PA14: STEP
    // PA15: SIDE_SELECT
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI0);   // EXTI0 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI0_PA;   // EXTI0 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI1);   // EXTI1 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI1_PA;   // EXTI1 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI2);   // EXTI2 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI2_PA;   // EXTI2 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI3);   // EXTI3 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI3_PA;   // EXTI3 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI12);  // EXTI12の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI12_PA;  // EXTI12を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI13);  // EXTI13の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI13_PA;  // EXTI13を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI14);  // EXTI14の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI14_PA;  // EXTI14を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI15);  // EXTI15の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI15_PA;  // EXTI15を PA (00) に設定

    // 一旦クリアしてから割り込みを有効にする
    EXTI->INTENR &= ~(EXTI_INTENR_MR0 | EXTI_INTENR_MR1 | EXTI_INTENR_MR2 | EXTI_INTENR_MR3 |      // 割り込み無効化
                      EXTI_INTENR_MR12 | EXTI_INTENR_MR13 | EXTI_INTENR_MR14 | EXTI_INTENR_MR15);  //
    EXTI->RTENR &= ~(EXTI_RTENR_TR0 | EXTI_RTENR_TR1 | EXTI_RTENR_TR2 | EXTI_RTENR_TR3 |           // 立ち上がりエッジ検出をクリア
                     EXTI_RTENR_TR12 | EXTI_RTENR_TR13 | EXTI_RTENR_TR14 | EXTI_RTENR_TR15);       //
    EXTI->FTENR &= ~(EXTI_FTENR_TR0 | EXTI_FTENR_TR1 | EXTI_FTENR_TR2 | EXTI_FTENR_TR3 |           // 立ち下がりエッジ検出をクリア
                     EXTI_FTENR_TR12 | EXTI_FTENR_TR13 | EXTI_FTENR_TR14 | EXTI_FTENR_TR15);       //

    // 有効化
    EXTI->RTENR |= EXTI_RTENR_TR0 | EXTI_RTENR_TR1 | EXTI_RTENR_TR2 | EXTI_RTENR_TR3 |     // 立ち上がりエッジ検出をセット
                   EXTI_RTENR_TR12 | EXTI_RTENR_TR13 | EXTI_RTENR_TR14 | EXTI_RTENR_TR15;  //
    EXTI->FTENR |= EXTI_FTENR_TR0 | EXTI_FTENR_TR1 | EXTI_FTENR_TR2 | EXTI_FTENR_TR3 |     // 立ち下がりエッジ検出をセット
                   EXTI_FTENR_TR12 | EXTI_FTENR_TR13 | EXTI_FTENR_TR14 | EXTI_FTENR_TR15;  //

    EXTI->INTFR = EXTI_INTF_INTF0 | EXTI_INTF_INTF1 | EXTI_INTF_INTF2 | EXTI_INTF_INTF3 |     // 割り込みフラグをクリア
                  EXTI_INTF_INTF12 | EXTI_INTF_INTF13 | EXTI_INTF_INTF14 | EXTI_INTF_INTF15;  //

    EXTI->INTENR |= EXTI_INTENR_MR0 | EXTI_INTENR_MR1 | EXTI_INTENR_MR2 | EXTI_INTENR_MR3 |     // 割り込み有効化
                    EXTI_INTENR_MR12 | EXTI_INTENR_MR13 | EXTI_INTENR_MR14 | EXTI_INTENR_MR15;  //

    NVIC_EnableIRQ(EXTI7_0_IRQn);   // EXTI 7-0割り込みを有効にする
    NVIC_EnableIRQ(EXTI15_8_IRQn);  // EXTI 15-8割り込みを有効にする
}

/*
  EXTI 7-0 Global Interrupt Handler
 */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
void EXTI7_0_IRQHandler(void) {
    exti_int_counter++;

    uint32_t intfr = EXTI->INTFR;  // 割り込みフラグを取得
    if (intfr & EXTI_INTF_INTF0) {
        // PA0 (DRIVE_SELECT_A) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF0;  // フラグをクリア
    }
    if (intfr & EXTI_INTF_INTF1) {
        // PA1 (DRIVE_SELECT_B) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF1;  // フラグをクリア
    }
    if (intfr & EXTI_INTF_INTF2) {
        // PA2 (OPTION_SELECT_A) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF2;  // フラグをクリア
    }
    if (intfr & EXTI_INTF_INTF3) {
        // PA3 (OPTION_SELECT_B) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF3;  // フラグをクリア
    }
}

void EXTI15_8_IRQHandler(void) __attribute__((interrupt));
void EXTI15_8_IRQHandler(void) {
    exti_int_counter++;
    uint32_t intfr = EXTI->INTFR;  // 割り込みフラグを取得
    uint32_t porta = GPIOA->INDR;
    if (intfr & EXTI_INTF_INTF12) {
        // PA12: MOTOR_ON の割り込み
        // PB4 : MOTOR_ON_DOSV output (論理逆) に反映する
        EXTI->INTFR = EXTI_INTF_INTF12;  // フラグをクリア
        if (porta & (1 << 12)) {
            // MOTOR_ONがHighになった
            GPIOB->BCR = (1 << 4);  // Motor ON
        } else {
            // MOTOR_ONがLowになった
            GPIOB->BSHR = (1 << 4);  // Motor OFF
        }
    }
    if (intfr & EXTI_INTF_INTF13) {
        // PA13: DIRECTION の割り込み
        // PB5 : DIRECTION_DOSV output (論理逆) に反映する
        EXTI->INTFR = EXTI_INTF_INTF13;  // フラグをクリア
        if (porta & (1 << 13)) {
            // DIRECTIONがHighになった
            GPIOB->BCR = (1 << 5);  // Direction反転
        } else {
            // DIRECTIONがLowになった
            GPIOB->BSHR = (1 << 5);  // Direction非反転
        }
    }
    if (intfr & EXTI_INTF_INTF14) {
        // PA14: STEP の割り込み
        // PB6 : STEP_DOSV output (論理逆) に反映する
        EXTI->INTFR = EXTI_INTF_INTF14;  // フラグをクリア
        if (porta & (1 << 14)) {
            // STEPがHighになった
            GPIOB->BCR = (1 << 6);  // Step High
        } else {
            // STEPがLowになった
            GPIOB->BSHR = (1 << 6);  // Step Low
        }
    }
    if (intfr & EXTI_INTF_INTF15) {
        // PA15: SIDE_SELECT の割り込み
        // PB7 : SIDE_SELECT_DOSV output (論理逆) に反映する
        EXTI->INTFR = EXTI_INTF_INTF15;  // フラグをクリア
        if (porta & (1 << 15)) {
            // SIDE_SELECTがHighになった
            GPIOB->BCR = (1 << 7);  // Side Select
        } else {
            // SIDE_SELECTがLowになった
            GPIOB->BSHR = (1 << 7);  // Side Select
        }
    }
}

void x68fdd_poll(uint32_t systick_ms) {
    // ポーリング処理をここに追加
    static uint64_t last_tick = 0;
    if (systick_ms - last_tick < 1000) {
        return;
    }
    last_tick = systick_ms;

    // OLEDに割り込み回数を表示する
    OLED_cursor(0, 6);
    OLED_printf("EXTI:%d", (int)exti_int_counter);
}