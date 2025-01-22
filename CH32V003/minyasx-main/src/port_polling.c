/*
 * SysTick IRQ Handlerを使って定期的に割り込みをかけ、ポートを監視する
 *
 * 本システムでは FDDの
 * DRIVE_SELECT信号を監視し、自分が選択された時に、適切な処理を行う必要がある。
 * また、DRIVE_SELECT信号がアクティブじゃなくなった場合は、速やかに信号のドライブを解除する必要がある。
 * メインループで行うと、処理が遅れる可能性があるため、割り込みを使って処理を行う。
 */
#include <stdio.h>

#include "ch32v003fun.h"

// Number of ticks elapsed per millisecond (48,000 when using 48MHz Clock)
#define SYSTICK_ONE_MILLISECOND ((uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000)
// Number of ticks elapsed per microsecond (48 when using 48MHz Clock)
#define SYSTICK_ONE_MICROSECOND ((uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000000)

void port_polling_init(void) {
    // Reset any pre-existing configuration
    SysTick->CTLR = 0x0000;

    // 10usec 単位で割り込みをかける、
    SysTick->CMP = 100 * SYSTICK_ONE_MICROSECOND - 1;

    // Reset the Count Register, and the global millis counter to 0
    SysTick->CNT = 0x00000000;

    // Set the SysTick Configuration
    // NOTE: By not setting SYSTICK_CTLR_STRE, we maintain compatibility with
    // busywait delay funtions used by ch32v003_fun.
    SysTick->CTLR |= SYSTICK_CTLR_STE |   // Enable Counter
                     SYSTICK_CTLR_STIE |  // Enable Interrupts
                     SYSTICK_CTLR_STCLK;  // Set Clock Source to HCLK/1

    // Enable the SysTick IRQ
    NVIC_EnableIRQ(SysTicK_IRQn);
}

uint32_t buzzer_counter = 0;

int drive_selected = 0;
int index_detected = 0;

/*
 * SysTick ISR - must be lightweight to prevent the CPU from bogging down.
 * Increments Compare Register and systick_millis when triggered (every 1ms)
 * NOTE: the `__attribute__((interrupt))` attribute is very important
 */
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void) {
    // Increment the Compare Register for the next trigger
    // If more than this number of ticks elapse before the trigger is reset,
    // you may miss your next interrupt trigger
    // (Make sure the IQR is lightweight and CMP value is reasonable)
    SysTick->CMP += 100 * SYSTICK_ONE_MICROSECOND;

    // Clear the trigger state for the next IRQ
    SysTick->SR = 0x00000000;

    // ここに割り込み処理を書く
    // ここでは、GPIOCのC5ピン(=DRIVE SELECT)を監視する

    // ピンの状態を読み取る
    uint32_t PC = GPIOC->INDR;
    uint32_t drive_select_n = (PC >> 5) & 1;

    if (drive_select_n) {
        // drive_select は負論理なので、ここは非選択状態
        // READY信号(A1)をOFFにする(READYはNOTバッファが入っているので正論理でOK)
        GPIOA->BCR = GPIO_Pin_1;
        GPIOD->BCR = GPIO_Pin_0; // DISK_IN_GEN(D0)をOFFにする
        GPIOD->BSHR = GPIO_Pin_2; // DRIVE_SELECT_DOSV_1(D2)をOFFにする
        GPIOD->BSHR = GPIO_Pin_3; // MODE_SELECT_DOSV(D3)をOFFにする
        drive_selected = 0;
        index_detected = 0;
    } else {
        // drive_select は選択状態
        GPIOD->BCR = GPIO_Pin_2; // DRIVE_SELECT_DOSV_1(D2)をONにする
        GPIOD->BCR = GPIO_Pin_3; // MODE_SELECT_DOSV(D3)をONにする
        buzzer_counter++;
        if (buzzer_counter >= 5 * 1000) {
            if (buzzer_counter >= 20 * 1000) {
                // 2秒経過したら、カウンタを戻す
                buzzer_counter = 0;
            }
        } else {
            if ((buzzer_counter % 20) == 9) {
                // 1msec経過したら、BUZZERをONにする
                GPIOA->BSHR = GPIO_Pin_2;
            }
            if ((buzzer_counter % 20) == 19) {
                // 2msec経過したら、BUZZERをOFFにする
                GPIOA->BCR = GPIO_Pin_2;
            }
        }

        // INDEX信号(C3)がアクティブなら、READY信号(A1)をONにする
        //if ((PC >> 3) & 1) {
            // 負論理なので、INDEXはアクティブじゃない
        //} else {
            // INDEXはアクティブ
            index_detected = 1;
            GPIOA->BSHR = GPIO_Pin_1;
            GPIOD->BSHR = GPIO_Pin_0; // DISK_IN_GEN(D0)をONにする
        //}
    }
    // FDD_INT_GEN(D7)をOFFにする
    GPIOD->BCR = GPIO_Pin_7;
    // DISK_IN_GEN(D0)をOFFにする
    //GPIOD->BCR = GPIO_Pin_0;
}