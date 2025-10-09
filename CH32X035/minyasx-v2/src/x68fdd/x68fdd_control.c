#include "x68fdd/x68fdd_control.h"

#include <stdbool.h>
#include <stdint.h>

#include "greenpak/greenpak_control.h"
#include "minyasx.h"
#include "pcfdd/pcfdd_control.h"
#include "ui/ui_control.h"

volatile uint32_t exti_int_counter = 0;

const bool double_option_A_always = false;  // 強制的にOPTION_SELECT_A/Bが両方アサートされるようにする
const bool double_option_B_always = false;  // 強制的にOPTION_SELECT_B/Bpairが両方アサートされるようにする

volatile bool double_option_A = double_option_A_always;
volatile bool double_option_B = double_option_B_always;

#define SYSTICK_INT_USEC 100  // SysTickの割り込み周期

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
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI0);  // EXTI0 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI0_PA;  // EXTI0 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI1);  // EXTI1 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI1_PA;  // EXTI1 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI2);  // EXTI2 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI2_PA;  // EXTI2 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI3);  // EXTI3 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI3_PA;  // EXTI3 を PA (00) に設定

    // 一旦クリアしてから割り込みを有効にする
    EXTI->INTENR &= ~(EXTI_INTENR_MR0 | EXTI_INTENR_MR1 | EXTI_INTENR_MR2 | EXTI_INTENR_MR3);  // 割り込み無効化
    EXTI->RTENR &= ~(EXTI_RTENR_TR0 | EXTI_RTENR_TR1 | EXTI_RTENR_TR2 | EXTI_RTENR_TR3);       // 立ち上がりエッジ検出をクリア
    EXTI->FTENR &= ~(EXTI_FTENR_TR0 | EXTI_FTENR_TR1 | EXTI_FTENR_TR2 | EXTI_FTENR_TR3);       // 立ち下がりエッジ検出をクリア

    // 有効化
    EXTI->RTENR |= EXTI_RTENR_TR0 | EXTI_RTENR_TR1 | EXTI_RTENR_TR2 | EXTI_RTENR_TR3;  // 立ち上がりエッジ検出をセット
    EXTI->FTENR |= EXTI_FTENR_TR0 | EXTI_FTENR_TR1;  // 立ち下がりエッジ検出をセット (OPTION_SELECT_A/Bは立ち上がりのみ)
    EXTI->INTFR = EXTI_INTF_INTF0 | EXTI_INTF_INTF1 | EXTI_INTF_INTF2 | EXTI_INTF_INTF3;    // 割り込みフラグをクリア
    EXTI->INTENR |= EXTI_INTENR_MR0 | EXTI_INTENR_MR1 | EXTI_INTENR_MR2 | EXTI_INTENR_MR3;  // 割り込み有効化

    NVIC_EnableIRQ(EXTI7_0_IRQn);  // EXTI 7-0割り込みを有効にする

    //
    // GPIO割り込みだけでは対応できない処理のために、SysTick割り込みを100usec単位で発生させる
    //
    // Reset any pre-existing configuration
    SysTick->CTLR = 0x0000;

    // 100usec 単位で割り込みをかける、
    SysTick->CMP = SYSTICK_INT_USEC * SYSTICK_ONE_MICROSECOND - 1;

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

    // GreenPAK4の Vitrual Input に以下を接続している
    // 7 (bit0)  = MOTOR_ON (正論理)
    // 6 (bit1)  = DIRECTION (正論理)
    // 5 (bit2)  = STEP (正論理)
    // 4 (bit3)  = SIDE_SELECT (正論理)
    greenpak_set_virtualinput(4 - 1, 0x00);  // 全部Lowにしておく
}

/*
  EXTI 7-0 Global Interrupt Handler
 */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
void EXTI7_0_IRQHandler(void) {
    uint32_t porta = GPIOA->INDR;
    uint32_t intfr = EXTI->INTFR;  // 割り込みフラグを取得

    exti_int_counter++;

    if (intfr & EXTI_INTF_INTF0) {
        // PA0 (DRIVE_SELECT_A) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF0;  // フラグをクリア
        if ((porta & (1 << 0))) {
            // DRIVE_SELECT_AがHigh(有効)になった
            if (double_option_A) {
                pcfdd_set_rpm_mode_select(&g_ctx->drive[0], FDD_RPM_300);
            } else {
                pcfdd_set_rpm_mode_select(&g_ctx->drive[0], FDD_RPM_360);
            }
            GPIOB->BCR = (1 << 3);            // DRIVE_SELECT_DOSV_B inactive (Low) to avoid both selected
            GPIOB->BSHR = (1 << 2);           // DRIVE_SELECT_DOSV_A active (High)
            pcfdd_set_current_ds(PCFDD_DS0);  // 現在のドライブ選択をAにセット
        } else {
            // DRIVE_SELECT_AがLow(無効)になった
            GPIOB->BCR = (1 << 2);                // DRIVE_SELECT_DOSV_A inactive (Low)
            pcfdd_set_current_ds(PCFDD_DS_NONE);  // 現在のドライブ選択をNoneにセット
        }
    }
    if (intfr & EXTI_INTF_INTF1) {
        // PA1 (DRIVE_SELECT_B) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF1;  // フラグをクリア
        if (porta & (1 << 1)) {
            // DRIVE_SELECT_BがHigh(有効)になった
            if (double_option_B) {
                pcfdd_set_rpm_mode_select(&g_ctx->drive[1], FDD_RPM_300);
            } else {
                pcfdd_set_rpm_mode_select(&g_ctx->drive[1], FDD_RPM_360);
            }
            GPIOB->BCR = (1 << 2);            // DRIVE_SELECT_DOSV_A inactive (Low) to avoid both selected
            GPIOB->BSHR = (1 << 3);           // DRIVE_SELECT_DOSV_B active (High)
            pcfdd_set_current_ds(PCFDD_DS1);  // 現在のドライブ選択をBにセット
        } else {
            // DRIVE_SELECT_BがLow(無効)になった
            GPIOB->BCR = (1 << 3);                // DRIVE_SELECT_DOSV_B inactive (Low)
            pcfdd_set_current_ds(PCFDD_DS_NONE);  // 現在のドライブ選択をNoneにセット
        }
    }
    if (intfr & EXTI_INTF_INTF2) {
        // PA2 (OPTION_SELECT_A) の割り込み (立ち上がりのみ)
        EXTI->INTFR = EXTI_INTF_INTF2;  // フラグをクリア
        // このタイミングで EJECT(PA4), EJECT_MASK(PA5), LED_BLINK(PA8)の状態を確認する
        drive_status_t* drive = &g_ctx->drive[0];  // Aドライブ
        if ((porta & (1 << 4)) == 0) {             // EJECT (Low=Eject)
            pcfdd_force_eject(g_ctx, 0);           // Aドライブを強制排出
        }
        if ((porta & (1 << 5)) == 0) {  // EJECT_MASK (Low=Mask)
            drive->eject_masked = true;
        } else {
            drive->eject_masked = false;
        }
        if ((porta & (1 << 8)) == 0) {  // LED_BLINK (Low=Blink)
            drive->led_blink = true;    // LEDが点滅中
        } else {
            drive->led_blink = false;  // LEDが点滅中でない
        }
    }
    if (intfr & EXTI_INTF_INTF3) {
        // PA3 (OPTION_SELECT_B) の割り込み (立ち上がりのみ)
        EXTI->INTFR = EXTI_INTF_INTF3;  // フラグをクリア
        // このタイミングで EJECT(PA4), EJECT_MASK(PA5), LED_BLINK(PA8)の状態を確認する
        drive_status_t* drive = &g_ctx->drive[1];  // Bドライブ
        if ((porta & (1 << 4)) == 0) {             // EJECT (Low=Eject)
            pcfdd_force_eject(g_ctx, 1);           // Bドライブを強制排出
        }
        if ((porta & (1 << 5)) == 0) {  // EJECT_MASK (Low=Mask)
            drive->eject_masked = true;
        } else {
            drive->eject_masked = false;
        }
        if ((porta & (1 << 8)) == 0) {  // LED_BLINK (Low=Blink)
            drive->led_blink = true;    // LEDが点滅中
        } else {
            drive->led_blink = false;  // LEDが点滅中でない
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
    SysTick->CMP = SysTick->CNT + SYSTICK_INT_USEC * SYSTICK_ONE_MICROSECOND;

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
    bool ds_a = (PA & GPIO_Pin_0);              // High active
    bool ds_b = (PA & GPIO_Pin_1);              // High active
    bool opt_a = (PA & GPIO_Pin_2) == 0;        // Low active
    bool opt_b = (PA & GPIO_Pin_3) == 0;        // Low active
    bool opt_a_pair = opt_b;                    // OPTION_SELECT_A のペアは OPTION_SELECT_B
    bool opt_b_pair = (PB & GPIO_Pin_11) == 0;  // OPTION_SELECT_B_PAIR

    //
    // OPTION_SELECT 同時アサートのローパスフィルタ
    //
    const uint32_t min_duration_assert = SYSTICK_ONE_MICROSECOND * 300;      // usec
    const uint32_t min_duration_deassert = SYSTICK_ONE_MICROSECOND * 30000;  // 30msec
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
            pcfdd_set_rpm_mode_select(&g_ctx->drive[0], FDD_RPM_300);
        } else {
            pcfdd_set_rpm_mode_select(&g_ctx->drive[0], FDD_RPM_360);
        }
    }
    if (ds_b) {
        if (double_option_B) {
            pcfdd_set_rpm_mode_select(&g_ctx->drive[1], FDD_RPM_300);
        } else {
            pcfdd_set_rpm_mode_select(&g_ctx->drive[1], FDD_RPM_360);
        }
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

    // EJECT, EJECT_MASK, LED_BLINKの監視はGPIO割り込みで行うのでここでは不要
}

void x68fdd_update_drive_id(minyasx_context_t* ctx) {
    // FDD IDの設定を決定
    uint8_t idA, idB;
    bool driveB_enabled = false;

    switch (ctx->preferences.fdd_id_mode) {
    case FDD_ID_MODE_DIP_SW: {
        // DIPスイッチから読み取る（従来の動作）
        uint8_t ds0 = (GPIOA->INDR >> 22) & 1;
        uint8_t ds1 = (GPIOA->INDR >> 23) & 1;
        idA = (ds1 << 1) | ds0;
        if (idA == 0 || idA == 2) {
            idB = idA + 1;
            driveB_enabled = true;
        }
        break;
    }
    case FDD_ID_MODE_0_1:
        idA = 0;
        idB = 1;
        driveB_enabled = true;
        break;
    case FDD_ID_MODE_1:
        idA = 1;
        break;
    case FDD_ID_MODE_2_3:
        idA = 2;
        idB = 3;
        driveB_enabled = true;
        break;
    case FDD_ID_MODE_3:
        idA = 3;
        break;
    default:
        idA = 0;
        idB = 1;
        driveB_enabled = true;
        break;
    }

    // 決定したIDをGreenPAKにセット
    // idA の値に応じてGreenPAKのVirtual Inputをセット
    // 0 -> ds1=0, ds0=0
    // 1 -> ds1=0, ds0=1
    // 2 -> ds1=1, ds0=0
    // 3 -> ds1=1, ds0=1
    uint8_t ds0 = idA & 1;
    uint8_t ds1 = (idA >> 1) & 1;
    for (int i = 0; i < 4; i++) {
        uint8_t vin = greenpak_get_virtualinput(i);
        vin &= ~0xC0;  // bit6, bit7 をクリア
        vin |= (ds1 ? 0x40 : 0x00) | (ds0 ? 0x80 : 0x00);
        greenpak_set_virtualinput(i, vin);
    }

    // ドライブIDを設定
    ctx->drive[0].drive_id = idA;
    ctx->drive[0].state = DRIVE_STATE_INITIALIZING;
    if (driveB_enabled) {
        ctx->drive[1].drive_id = idB;
        ctx->drive[1].state = DRIVE_STATE_INITIALIZING;
    } else {
        ctx->drive[1].state = DRIVE_STATE_DISABLED;
    }
}