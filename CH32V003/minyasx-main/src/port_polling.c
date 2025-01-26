/*
 * SysTick IRQ Handlerを使って定期的に割り込みをかけ、ポートを監視する
 *
 * 本システムでは FDDの
 * DRIVE_SELECT信号を監視し、自分が選択された時に、適切な処理を行う必要がある。
 * また、DRIVE_SELECT信号がアクティブじゃなくなった場合は、速やかに信号のドライブを解除する必要がある。
 * メインループで行うと、処理が遅れる可能性があるため、割り込みを使って処理を行う。
 */
#include "port_polling.h"

#include <stdio.h>

#include "ch32v003fun.h"
#include "main.h"

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

bool drive_selected = 0;
uint32_t last_index_detected = 0;

/**
 * @brief 2HDDの回転数を決めるフラグ
 *
 * 通常(=0)はX68000に合わせて1.2MBの回転数を使うが、OPTION_SELECTの同時アサートにより
 * 1.44MBの回転数を使うことができる(=1)。
 * この信号は3mode driveの
 * MODE_SELECTに接続されるが、ドライブの設定や機種によっては
 * 論理が逆になるケースがある。この設定は、mode_select_invertで行う。
 * mode_select_invertがアサートされている場合は、出力信号の論理を反転するが、
 * この revolution_2hd_144 は常に 0が1.2MB、1が1.44MBを表す。
 */
bool revolution_2hd_144 = 0;  // 0: 1.2MB, 1: 1.44MB

// チャタリング対策用のフラグ
bool option_both_asserted_prev = 0;

uint32_t revolution_2hd_144_lastsame = 0;

// エッジ検出用のフラグ
bool option_asserted_prev = 0;
uint32_t PD_prev;

/*
 * revolution_2hdの値が変化した時刻を保持するフラグ
 * 回転数が変更されてから、0.5秒(500msec)の間はモーターの回転が安定しないため、
 * このフラグが立っている間は、READY信号をアクティブにしないようにする
 */
uint32_t revolution_2hd_changed = 0;

bool index_detected = 0;

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

        GPIOA->BCR = GPIO_Pin_1;   // READY信号(A1)をOFFにする
                                   // (NOTバッファがあるので正論理)
        GPIOD->BCR = GPIO_Pin_0;   // DISK_IN_GEN(D0)をOFFにする
                                   // (NOTバッファがあるので正論理)
        GPIOD->BSHR = GPIO_Pin_2;  // DRIVE_SELECT_DOSV_1(D2)をOFFにする
        drive_selected = 0;
        in_access = 0;
        index_detected = 0;
    } else {
        // drive_select は選択状態
        in_access = 1;
        GPIOD->BCR = GPIO_Pin_2;  // DRIVE_SELECT_DOSV_1(D2)をONにする

        // OPTION_SELECT, OPTION_SELECT_PAIRの両方がアサートされていたら
        // MODE_SELECTをActive (Low) にして300RPMに、1.44MBモードにする
        // そうでなければMODE_SELECTをInactive (High)
        // にして360RPMに、1.2KBモードにする
        // ただし、mode_select_invertがアサートされていたら、論理を逆にする
        bool revolution_changed = false;
        bool option_both_asserted_now = (((PC >> 6) & 1) == 0) && (((PC >> 7) & 1) == 0) ? 1 : 0;
        if (option_both_asserted_now != revolution_2hd_144) {
            // 1ms以上変化が続いたら、安定して変化したとみなし、受け入れる
            if (SysTick->CNT - revolution_2hd_144_lastsame > 1 * SYSTICK_ONE_MILLISECOND) {
                revolution_changed = true;
                revolution_2hd_144 = option_both_asserted_now;
                revolution_2hd_144_lastsame = SysTick->CNT;
            }
        } else {
            // 同じ値だったら、変化がないとみなし、時刻を記録
            revolution_2hd_144_lastsame = SysTick->CNT;
        }

        // 回転数の変化要求があった場合、MODE_SELECT信号を変更する
        if (revolution_changed) {
            revolution_2hd_changed = SysTick->CNT;
            // OPTION_SELECT, OPTION_SELECT_PAIRの両方がアサートされたら、
            if (revolution_2hd_144) {
                if (mode_select_invert) {
                    // 反転時はMODE_SELECT_DOSV(D3)をインアクティブ(High)にすると1.44MB
                    GPIOD->BSHR = GPIO_Pin_3;
                } else {
                    // 通常時はMODE_SELECT_DOSV(D3)をアクティブ(Low)にすると1.44MB
                    GPIOD->BCR = GPIO_Pin_3;
                }
            } else {
                if (mode_select_invert) {
                    // 反転時はMODE_SELECT_DOSV(D3)をアクティブ(Low)にすると1.2MB
                    GPIOD->BCR = GPIO_Pin_3;
                } else {
                    // 通常時はMODE_SELECT_DOSV(D3)をインアクティブ(High)にすると1.2MB
                    GPIOD->BSHR = GPIO_Pin_3;
                }
            }
        }

        // INDEX信号(C3)がアクティブかどうかを検出
        if (((PC >> 3) & 1) == 0) {
            // INDEXはアクティブ
            index_detected = 1;
            last_index_detected = SysTick->CNT;
            media_inserted = 1;  // TODO: DISK_CHANGE_DOSV(C1)がアクティブなら未挿入にする
        }

#if 0
        // READY信号(A1)を生成する
        // * 0.5秒以内に回転数が変更されていたら、READY信号をアクティブにしない
        // * DRIVE_SELECT中にINDEXが検出されていたら、READY信号をアクティブにする
        if (SysTick->CNT - revolution_2hd_changed > 500 * SYSTICK_ONE_MILLISECOND) {
            if (index_detected) {
                // indexが検出されていたら、READY信号(A1)とDISK_IN_GEN(D0)をONにする
                GPIOA->BSHR = GPIO_Pin_1;
                GPIOD->BSHR = GPIO_Pin_0;
            } else {
                // indexが1秒以上検出されていなかったら、READY信号(A1)とDISK_IN_GEN(D0)をOFFにする
                GPIOA->BCR = GPIO_Pin_1;
                GPIOD->BCR = GPIO_Pin_0;
            }
        }
#elsif 0
        // メディアが挿入されているならREADY信号をアクティブにする
        if( media_inserted && index_detected) {
            // READY信号(A1)とDISK_IN_GEN(D0)をONにする
            GPIOA->BSHR = GPIO_Pin_1;
            GPIOD->BSHR = GPIO_Pin_0;
        } else {
            // READY信号(A1)とDISK_IN_GEN(D0)をOFFにする
            GPIOA->BCR = GPIO_Pin_1;
            GPIOD->BCR = GPIO_Pin_0;
        }
#else
        // MOTOR ON信号(C4)がアクティブならREADY信号をアクティブにする
        if (((PC >> 4) & 1) == 0) {
            // MOTOR ON信号がアクティブ
            GPIOA->BSHR = GPIO_Pin_1;
            GPIOD->BSHR = GPIO_Pin_0;
        } else {
            // MOTOR ON信号が非アクティブ
            GPIOA->BCR = GPIO_Pin_1;
            GPIOD->BCR = GPIO_Pin_0;
        }
#endif

#if BUZZER_ENABLE
        buzzer_counter++;
        int buzzer_on = 0;
        // 0.05秒ごとに鳴らす/鳴らさないを切り替える
        switch (buzzer_counter / (100 * 5)) {
            case 0:
                buzzer_on = 1;
                break;
            case 1:
                buzzer_on = revolution_2hd_144 ? 0 : 1;
                break;
            case 2:
                buzzer_on = 1;
                break;
            case 19:
                buzzer_counter = 0;
                break;
            default:
                buzzer_on = 0;
                break;
        }

        if (buzzer_on) {
            if ((buzzer_counter % 20) == 9) {
                // 1msec経過したら、BUZZERをONにする
                GPIOA->BSHR = GPIO_Pin_2;
            }
            if ((buzzer_counter % 20) == 19) {
                // 2msec経過したら、BUZZERをOFFにする
                GPIOA->BCR = GPIO_Pin_2;
            }
        } else {
            GPIOA->BCR = GPIO_Pin_2;
        }
#endif
    }

    // OPTION信号のチェック
    bool option_asserted = ((PC >> 7) & 1) == 0;
    if (!option_asserted && option_asserted_prev) {
        // OPTION_SELECTの立ち上がりエッジで、以下の信号を受信
        // D4: EJECT_MASK,          Input, pull-up
        // D5: EJECT,               Input, pull-up
        // D6: LED_BLINK,           Input, pull-up
        eject_mask = ((PD_prev >> 4) & 1) == 0;
        eject = ((PD_prev >> 5) & 1) == 0;
        led_blink = ((PD_prev >> 6) & 1) == 0;
        if (eject) {
            media_inserted = 0;
        }
    }
    option_asserted_prev = option_asserted;
    PD_prev = GPIOD->INDR;

    // FDD_INT_GEN(D7)をOFFにする
    GPIOD->BCR = GPIO_Pin_7;
    // DISK_IN_GEN(D0)をOFFにする
    // GPIOD->BCR = GPIO_Pin_0;
}