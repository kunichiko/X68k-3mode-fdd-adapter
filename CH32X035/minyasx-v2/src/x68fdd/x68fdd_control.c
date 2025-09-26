#include "x68fdd/x68fdd_control.h"

#include <stdbool.h>
#include <stdint.h>

#include "pcfdd/pcfdd_control.h"
#include "ui/ui_control.h"

volatile uint32_t exti_int_counter = 0;

const bool double_option_A_always = true;   // OPTION_SELECT_Aが両方アサートされるようにする
const bool double_option_B_always = false;  // OPTION_SELECT_Bが両方アサートされるようにする

volatile bool double_option_A = double_option_A_always;
volatile bool double_option_B = double_option_B_always;

// Number of ticks elapsed per millisecond (48,000 when using 48MHz Clock)
#define SYSTICK_ONE_MILLISECOND ((uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000)
// Number of ticks elapsed per microsecond (48 when using 48MHz Clock)
#define SYSTICK_ONE_MICROSECOND ((uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000000)

// 割り込みルーチンからコンテキストを参照できるようにする
static minyasx_context_t* g_ctx = NULL;

void x68fdd_init(minyasx_context_t* ctx) {
    g_ctx = ctx;
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

    //
    // GPIO割り込みだけでは対応できない処理のために、SysTick割り込みを100usec単位で発生させる
    //
    // Reset any pre-existing configuration
    SysTick->CTLR = 0x0000;

    // 100usec 単位で割り込みをかける、
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

    // GP ENABLE
    // GPIOC->BSHR = (1 << 6);  // GP_ENABLE (High=Enable)
    GPIOC->BCR = (1 << 6);  // GP_ENABLE (Low=Disable)
}

/*
  EXTI 7-0 Global Interrupt Handler
 */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
void EXTI7_0_IRQHandler(void) {
    exti_int_counter++;

    uint32_t intfr = EXTI->INTFR;  // 割り込みフラグを取得
    uint32_t porta = GPIOA->INDR;
    if (intfr & EXTI_INTF_INTF0) {
        // PA0 (DRIVE_SELECT_A) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF0;  // フラグをクリア
        if (porta & (1 << 0)) {
            // DRIVE_SELECT_AがHighになった
            GPIOB->BCR = (1 << 2);                // DRIVE_SELECT_DOSV_A inactive (Low)
            pcfdd_set_current_ds(PCFDD_DS_NONE);  // 現在のドライブ選択をNoneにセット
        } else {
            // DRIVE_SELECT_AがLowになった
            if (double_option_A) {
                set_mode_select(&g_ctx->drive[0], FDD_RPM_300);
            } else {
                set_mode_select(&g_ctx->drive[0], FDD_RPM_360);
            }
            GPIOB->BCR = (1 << 3);            // DRIVE_SELECT_DOSV_B inactive (Low) to avoid both selected
            GPIOB->BSHR = (1 << 2);           // DRIVE_SELECT_DOSV_A active (High)
            pcfdd_set_current_ds(PCFDD_DS0);  // 現在のドライブ選択をAにセット
        }
    }
    if (intfr & EXTI_INTF_INTF1) {
        // PA1 (DRIVE_SELECT_B) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF1;  // フラグをクリア
        if (porta & (1 << 1)) {
            // DRIVE_SELECT_BがHighになった
            GPIOB->BCR = (1 << 3);                // DRIVE_SELECT_DOSV_B inactive (Low)
            pcfdd_set_current_ds(PCFDD_DS_NONE);  // 現在のドライブ選択をNoneにセット
        } else {
            // DRIVE_SELECT_BがLowになった
            if (double_option_B) {
                set_mode_select(&g_ctx->drive[1], FDD_RPM_300);
            } else {
                set_mode_select(&g_ctx->drive[1], FDD_RPM_360);
            }
            GPIOB->BCR = (1 << 2);            // DRIVE_SELECT_DOSV_A inactive (Low) to avoid both selected
            GPIOB->BSHR = (1 << 3);           // DRIVE_SELECT_DOSV_B active (High)
            pcfdd_set_current_ds(PCFDD_DS1);  // 現在のドライブ選択をBにセット
        }
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
            GPIOB->BCR = (1 << 4);  // Motor ON inactive
        } else {
            // MOTOR_ONがLowになった
            GPIOB->BSHR = (1 << 4);  // Motor ON active
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
            GPIOB->BCR = (1 << 6);  // Step inactive
        } else {
            // STEPがLowになった
            GPIOB->BSHR = (1 << 6);  // Step active
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

volatile uint32_t systick_irq_counter = 0;

volatile uint32_t double_option_A_time = 0;
volatile uint32_t double_option_B_time = 0;

/*
 * SysTick ISR - must be lightweight to prevent the CPU from bogging down.
 * Increments Compare Register and systick_millis when triggered (every 1ms)
 * NOTE: the `__attribute__((interrupt))` attribute is very important
 */
void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void) {
    systick_irq_counter++;
    // Increment the Compare Register for the next trigger
    // If more than this number of ticks elapse before the trigger is reset,
    // you may miss your next interrupt trigger
    // (Make sure the IQR is lightweight and CMP value is reasonable)
    SysTick->CMP = SysTick->CNT + 100 * SYSTICK_ONE_MICROSECOND;

    // Clear the trigger state for the next IRQ
    SysTick->SR = 0x00000000;

    // 9SCDRVサポート
    // OPTION SELECT 信号の同時アサートによる回転数変更に対応する
    // ●戦略
    // 9SCDRVは 300RPMにする際に OPTION SELECT A/Bを同時にアサートします
    // しかし、グリッジが出ることがあるので、その変化に過敏に反応して MODE_SELECT_DOSVを切り替えると
    // ドライブがついてこず、うまくうごきません。
    // そこで、条件を以下のように少し厳しくします。
    //  * まず、以下のそれぞれに対し「1msec以上維持した場合」という制限をかけ、その状態を double_option 変数に保存する
    //    - OPTION同時未選択 → 同時選択への変化
    //    - OPTION同時選択 → 同時未選択への変化
    //  * DRIVE_SELECT がアサートされていない場合を初期状態(IDLE)とする
    //  * DRIVE_SELECTがアサートされているあいだは、定期的に double_option の状態を監視し、その対に応じて MODE_SELECT_DOSVを切り替える
    //  * その際、各ドライブの最後の回転数モードを記憶しておき、次回DRIVE＿SELECTがアサートされたときはそれに応じてMODE_SELECT_DOSVを設定する
    uint32_t PA = GPIOA->INDR;
    uint32_t PB = GPIOB->INDR;
    bool ds_a = (PA & GPIO_Pin_0) == 0;         // Low active
    bool ds_b = (PA & GPIO_Pin_1) == 0;         // Low active
    bool opt_a = (PA & GPIO_Pin_2) == 0;        // Low active
    bool opt_b = (PA & GPIO_Pin_3) == 0;        // Low active
    bool opt_a_pair = opt_b;                    // OPTION_SELECT_A のペアは OPTION_SELECT_B
    bool opt_b_pair = (PB & GPIO_Pin_11) == 0;  // OPTION_SELECT_B_PAIR

    //
    // OPTION_SELECT 同時アサートのローパスフィルタ
    //
    const uint32_t min_duration_assert = SYSTICK_ONE_MICROSECOND * 300;      // usec
    const uint32_t min_duration_deassert = SYSTICK_ONE_MICROSECOND * 30000;  // 30msec
    uint32_t motor_change_tickA = 0;
    uint32_t motor_change_tickB = 0;
    (void)motor_change_tickA;
    (void)motor_change_tickB;
    if (!double_option_A) {
        if (opt_a && opt_a_pair) {
            // OPTION SELECT Aとペアの両方がアサートされたので、計測
            if (double_option_A_time == 0) {
                // 最初のアサート
                double_option_A_time = SysTick->CNTL;
            } else if ((SysTick->CNTL - double_option_A_time) > min_duration_assert) {
                // 一定期間継続している
                double_option_A = true;
                double_option_A_time = 0;
                motor_change_tickA = SysTick->CNTL;
            } else {
                // まだ一定期間に達していない
            }
        } else {
            double_option_A_time = 0;
        }
    } else {
        if (!(opt_a && opt_a_pair)) {
            // OPTION SELECT Aのどちらかがディアサートされたので、計測
            if (double_option_A_time == 0) {
                // 最初のディアサート
                double_option_A_time = SysTick->CNTL;
            } else if ((SysTick->CNTL - double_option_A_time) > min_duration_deassert) {
                // 一定期間継続している
                double_option_A = double_option_A_always || false;
                double_option_A_time = 0;
                motor_change_tickA = SysTick->CNTL;
            } else {
                // まだ一定期間に達していない
            }
        } else {
            double_option_A_time = 0;
        }
    }

    if (!double_option_B) {
        if (opt_b && opt_b_pair) {
            // OPTION SELECT Bとペアの両方がアサートされたので、計測
            if (double_option_B_time == 0) {
                // 最初のアサート
                double_option_B_time = SysTick->CNTL;
            } else if ((SysTick->CNTL - double_option_B_time) > min_duration_assert) {
                // 一定期間継続している
                double_option_B = true;
                double_option_B_time = 0;
                motor_change_tickB = SysTick->CNTL;
            } else {
                // まだ一定期間に達していない
            }
        } else {
            double_option_B_time = 0;
        }
    } else {
        if (!(opt_b && opt_b_pair)) {
            // OPTION SELECT Bのどちらかがディアサートされたので、計測
            if (double_option_B_time == 0) {
                // 最初のディアサート
                double_option_B_time = SysTick->CNTL;
            } else if ((SysTick->CNTL - double_option_B_time) > min_duration_deassert) {
                // 一定期間継続している
                double_option_B = double_option_B_always || false;
                double_option_B_time = 0;
                motor_change_tickB = SysTick->CNTL;
            } else {
                // まだ一定期間に達していない
            }
        } else {
            double_option_B_time = 0;
        }
    }

    //
    // DRIVE_SELECTの状態に応じてMODE_SELECT_DOSVを切り替える
    //
    if (ds_a) {
        if (double_option_A) {
            // DRIVE_SELECT_Aがアサートされていて、OPTION SELECT Aが同時アサートされている
            set_mode_select(&g_ctx->drive[0], FDD_RPM_300);
        } else {
            set_mode_select(&g_ctx->drive[0], FDD_RPM_360);
        }
    }
    if (ds_b) {
        if (double_option_B) {
            set_mode_select(&g_ctx->drive[1], FDD_RPM_300);
        } else {
            set_mode_select(&g_ctx->drive[1], FDD_RPM_360);
        }
    }

    // MOTOR ON信号(PA12)がアクティブならREADY信号をアクティブにする
    // GreenPAKは各ドライブにDriveSelect信号がアサートされると、
    // このREADY信号の値を返却します
    if (!(GPIOA->INDR & GPIO_Pin_12)) {
        // MOTOR ON信号がアクティブ
#if 0
        if ((motor_change_tickA != 0) &&  //
            (SysTick->CNTL - motor_change_tickA < 3 * SYSTICK_ONE_MILLISECOND)) {
            // OPTION_SELECT_Aの変化によりMODE_SELECT_DOSVを変更した直後
            // READY信号を非アクティブにしておく
            GPIOB->BSHR = GPIO_Pin_12;  // READY_MCU_A_n (High=準備完了でない)
        } else {
            GPIOB->BCR = GPIO_Pin_12;  // READY_MCU_A_n (Low=準備完了)
            motor_change_tickA = 0;
        }
        if ((motor_change_tickB != 0) &&  //
            (SysTick->CNTL - motor_change_tickB < 3 * SYSTICK_ONE_MILLISECOND)) {
            // OPTION_SELECT_Bの変化によりMODE_SELECT_DOSVを変更した直後
            // READY信号を非アクティブにしておく
            GPIOB->BSHR = GPIO_Pin_13;  // READY_MCU_B_n (High=準備完了でない)
        } else {
            GPIOB->BCR = GPIO_Pin_13;  // READY_MCU_B_n (Low=準備完了)
            motor_change_tickB = 0;
        }
#elif 0
        // 戦略2
        // 各ドライブのMODE_SELECTによる回転数の設定状況を考慮し、
#else
        GPIOB->BCR = GPIO_Pin_12;  // READY_MCU_A_n (Low=準備完了)
        GPIOB->BCR = GPIO_Pin_13;  // READY_MCU_B_n (Low=準備完了)
#endif
    } else {
        // MOTOR ON信号が非アクティブ
        GPIOB->BSHR = GPIO_Pin_12;  // READY_MCU_A_n (High=準備完了でない)
        GPIOB->BSHR = GPIO_Pin_13;  // READY_MCU_B_n (High=準備完了でない)
        motor_change_tickA = 0;
        motor_change_tickB = 0;
    }
}

void x68fdd_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // ポーリング処理をここに追加
    static uint32_t last_tick = 0;
    if (systick_ms - last_tick < 1000) {
        return;
    }
    last_tick = systick_ms;

    // OLEDに割り込み回数を表示する
    // ui_cursor(UI_PAGE_DEBUG, 0, 6);
    // ui_printf(UI_PAGE_DEBUG, "EXTI:%d", (int)exti_int_counter);

    // OPTION_SELECT_A/Bの状態をOLEDに表示する
    ui_cursor(UI_PAGE_DEBUG, 0, 7);
    uint8_t opt_a = (GPIOA->INDR & GPIO_Pin_2) ? 1 : 0;
    uint8_t opt_b = (GPIOA->INDR & GPIO_Pin_3) ? 1 : 0;
    uint8_t opt_a_pair = opt_b;  // OPTION_SELECT_A のペアは OPTION_SELECT_B
    uint8_t opt_b_pair = (GPIOB->INDR & GPIO_Pin_11) ? 1 : 0;
    uint8_t amode = double_option_A ? 'Q' : 'D';
    uint8_t bmode = double_option_B ? 'Q' : 'D';
    ui_printf(UI_PAGE_DEBUG, "OP A%d%d%c B%d%d%c %d", opt_a, opt_a_pair, amode, opt_b, opt_b_pair, bmode, systick_irq_counter);
}