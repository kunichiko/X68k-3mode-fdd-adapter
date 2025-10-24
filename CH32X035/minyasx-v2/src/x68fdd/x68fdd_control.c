/*
X68000側からのアクセスに対し、以下のような処理を行います。

● DRIVE_SELECT
DRIVE_SELECT は以下の用途で使用します。
- 今アクセスしているのがA/Bどちらのドライブかの判定
- ドライブの回転数制御(MODE_SELECT)信号の送出

◯ ドライブ判定
BPS検出やRPM検出など、ドライブA/Bそれぞれで独立して行う必要がある処理において、
現在X68000側からアクセスされているドライブがAなのかBなのかを判定するために使用します。

◯ 回転数制御
回転数制御のためには、DRIVE_SELECT時にMODE_SELECT信号を適切に設定する必要があります。
ドライブAとBがそれぞれ異なる回転数になることもあるため、事前にOPTION_SELECT信号の状態を確認し、
それに応じたMODE_SELECT信号の設定を行います。
この処理はハード(GreenPAK)で行うようにしておけばよかったのですが、
設計時にそこまで気が回らなかったので、マイコンで処理することにしました。
こちらは、DRIVE_SELECTのGPIO割り込みで対応します。
→設計変更。GreenPAKで処理するようにしました。

● OPTION_SELECT
OPTION_SELECT は以下の用途で使用します。
- EJECT, EJECT_MASK, LED_BLINK信号のラッチ
- 9SCDRVモードでの回転数制御 (OPTION_SELECTの2本同時アサート)

◯ EJECT, EJECT_MASK, LED_BLINK信号のラッチ
これらの信号は OPTION_SELECT_A/Bがディアサートされたタイミングでラッチする必要があります。
これもハード的にやっておけば楽だったのですが、設計時にそこまで気が回らなかったので、こちらもマイコンで
処理することにしました。
ただ、マイコンだと「ある信号の立ち上がりエッジで値を取り込む」という処理が簡単にはできません。
今回は以下のようにタイマーとDMAを使って処理することにします。
- OPTION_SELECT_A/Bは TIM2CH3, TIM2CH4 にそれぞれ接続されている
- これらのタイマー入力は、DMAのChannel1, Channel7のトリガとして使用できる
- DMAはそのトリガを受けて GPIOAのIDRレジスタを読み込み、指定されたメモリ(リングバッファ)に書き込むことができる
- CPUは定期的にそのリングバッファをチェックし、新しいデータがあれば処理する

◯ OPTION_SELECTの2本同時アサート
- ドライブA: OPTION_SELECT_A/Bが両方アサートされた場合
- ドライブB: OPTION_SELECT_B/B_Pairが両方アサートされた場合
これらの信号が両方アサートされた場合、ドライブの回転数を300RPMに設定します。
MODE_SELECT信号はドライブによってどちらが300RPM/360RPMになるかが異なるため、
設定値(drive->mode_select_inverted)に応じてMODE_SELECT信号を設定します。
この同時アサートは9scdrvが行ってくれるものですが、以下のような特徴があるので、
それに対応した処理を行います。
- 同時アサートはDRIVE_SELECTと非同期で行われる(可能性がある)
- 同時アサートが瞬間的に途切れることがある

9SCDRVのパッケージ(9SCSET)に含まれる9SCHRD.DOCにある回路によると、
OPTION_SELECTの同時アサートを検出した場合に、FDDのLSIの回転数制御端子を直接制御
するようになっています。つまり、DRIVE_SELECT状態に関わらず、MOTORO_ON状態であれば
回転数が変化することになります。
一方、DOS/V用のFDDの場合、モーターの回転数を直接いじることはできず、あくまで
DRIVE_SELECT時にMODE_SELECT信号を切り替えることで回転数を制御する必要があります。
そのため、Minyas Xでは、以下のように対応します。
- OPTION_SELECTの同時アサート状態を常に監視し、ドライブA/Bそれぞれの状態をフラグに保持する
- DRIVE_SELECT発生したら、そのフラグに応じて即座にMODE_SELECT信号を切り替える

さらに、OPTION_SELECTの同時アサートは瞬間的に途切れることがあるのが経験的に分かっています。
ですが、モーター回転数はすぐには変化しないため、OPTION_SELECTの同時アサートが途切れた場合でも
しばらくはその回転数が維持されるため、瞬間的に途切れても特に問題はなかったようです。
一方、その途切れに対しMODE_SELECT信号を即座に変化させてしまうと、DOS/V用FDDでは問題が発生します。
(MODE_SELECT信号が変わると回転数変更状態に遷移しようとしてしまい、アクセスを受け付けなくなる)
このような問題があるため、OPTION_SELECTの同時アサートは即座に応答せず、ヒステリシスな特性で
応答するようにします。(ディアサート→アサートは数百μs程度の遅延を入れ、アサート→ディアサートは
数百ms程度の遅延を入れる)

なおMODE_SELECT信号は DRIVE_SELECTのアサートと同時(それより前)に設定しておくことが理想です。
DRIVE_SELECT中にモードを変更すること自体は可能ですが、回転数が安定するまでしばらく待たされたり、
前述のように、瞬間的な変化は誤動作を引き起こす可能性があります。

特に、9SCDRVがない状態であっても、

* ドライブAは MODE_SELECTが High (ディアサート状態) で360RPM、Lowで300RPM
* ドライブBは MODE_SELECTが Low (アサート状態) で360RPM、Highで300RPM

という組み合わせだった場合に、ドライブAとBを交互にアクセスすると、MODE_SELECT信号が頻繁にかつ
確実に変化させる必要があります。

これをマイコンの割り込みで行うのは難しいため、MODE_SELECT信号はGreenPAKが生成することにします。
具体的には以下のような動作をします。

* OPTION_SELECTの同時アサートの監視自体はマイコンが行う
* 同時アサートを検出したら、GreenPAKのVirtual Inputの設定を変更して、各ドライブの MODE_SELECT信号レベルをセットする
    * Virtual Inputは I2Cで行うので数十μs程度の遅延が発生する可能性があるが、そもそもモーターの回転数を変えた場合は
      数百ms程度の遅延が発生するので問題ないと判断
* 同時アサートが解除された場合は、念のため 30msec 後に Virtual Input の設定を元に戻す (ヒステリシス特性の実現)
* GreenPAKは、DRIVE_SELECT時に Virtual Input の設定に応じて MODE_SELECT 信号を出力する
* GreenPAKは、DELAY回路を使って DRIVE_SELECTを僅かに遅延させ、MODE_SELECT信号が先に確定するようにする

このようにすることで、MODE_SELECTを確実にセットできるようになります。

*/
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
    //
    //
    // PA0 : DRIVE_SELECT_A
    // PA1 : DRIVE_SELECT_B
    // PA2 : OPTION_SELECT_A
    // PA3 : OPTION_SELECT_B
    // PA13: EJECT (V2.0はPA4)
    // PA14: EJECT_MASK (V2.0はPA5)
    // PA15: LED_BLINK (V2.0はPA8)
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI0);   // EXTI0 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI0_PA;   // EXTI0 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI1);   // EXTI1 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI1_PA;   // EXTI1 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI2);   // EXTI2 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI2_PA;   // EXTI2 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI3);   // EXTI3 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI3_PA;   // EXTI3 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI4);   // EXTI4 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI4_PA;   // EXTI4 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI5);   // EXTI5 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI5_PA;   // EXTI5 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI8);   // EXTI8 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI8_PA;   // EXTI8 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI13);  // EXTI13 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI13_PA;  // EXTI13 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI14);  // EXTI14 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI14_PA;  // EXTI14 を PA (00) に設定
    AFIO->EXTICR1 &= ~(AFIO_EXTICR1_EXTI15);  // EXTI15 の設定をクリア
    AFIO->EXTICR1 |= AFIO_EXTICR1_EXTI15_PA;  // EXTI15 を PA (00) に設定

    // 一旦クリアしてから割り込みを有効にする
    EXTI->INTENR &= ~(EXTI_INTENR_MR0 | EXTI_INTENR_MR1 | EXTI_INTENR_MR2 | EXTI_INTENR_MR3 |  //
                      EXTI_INTENR_MR4 | EXTI_INTENR_MR5 | EXTI_INTENR_MR8 |                    //
                      EXTI_INTENR_MR13 | EXTI_INTENR_MR14 | EXTI_INTENR_MR15);                 // 割り込み無効化
    EXTI->RTENR &= ~(EXTI_RTENR_TR0 | EXTI_RTENR_TR1 | EXTI_RTENR_TR2 | EXTI_RTENR_TR3 |       //
                     EXTI_RTENR_TR4 | EXTI_RTENR_TR5 | EXTI_RTENR_TR8 |                        //
                     EXTI_RTENR_TR13 | EXTI_RTENR_TR14 | EXTI_RTENR_TR15);                     // 立ち上がりエッジ検出をクリア
    EXTI->FTENR &= ~(EXTI_FTENR_TR0 | EXTI_FTENR_TR1 | EXTI_FTENR_TR2 | EXTI_FTENR_TR3 |       //
                     EXTI_FTENR_TR4 | EXTI_FTENR_TR5 | EXTI_FTENR_TR8 |                        //
                     EXTI_FTENR_TR13 | EXTI_FTENR_TR14 | EXTI_FTENR_TR15);                     // 立ち下がりエッジ検出をクリア

    // 有効化
    EXTI->RTENR |= EXTI_RTENR_TR0 | EXTI_RTENR_TR1 | EXTI_RTENR_TR2 | EXTI_RTENR_TR3 |       //
                   EXTI_RTENR_TR4 | EXTI_RTENR_TR5 | EXTI_RTENR_TR8 |                        //
                   EXTI_RTENR_TR13 | EXTI_RTENR_TR14 | EXTI_RTENR_TR15;                      // 立ち上がりエッジ検出をセット
    EXTI->FTENR |= EXTI_FTENR_TR0 | EXTI_FTENR_TR1 | EXTI_FTENR_TR2 | EXTI_FTENR_TR3 |       //
                   EXTI_FTENR_TR4 | EXTI_FTENR_TR5 | EXTI_FTENR_TR8 |                        //
                   EXTI_FTENR_TR13 | EXTI_FTENR_TR14 | EXTI_FTENR_TR15;                      // 立ち下がりエッジ検出をセット
    EXTI->INTFR = EXTI_INTF_INTF0 | EXTI_INTF_INTF1 | EXTI_INTF_INTF2 | EXTI_INTF_INTF3 |    //
                  EXTI_INTF_INTF4 | EXTI_INTF_INTF5 | EXTI_INTF_INTF8 |                      //
                  EXTI_INTF_INTF13 | EXTI_INTF_INTF14 | EXTI_INTF_INTF15;                    // 割り込みフラグをクリア
    EXTI->INTENR |= EXTI_INTENR_MR0 | EXTI_INTENR_MR1 | EXTI_INTENR_MR2 | EXTI_INTENR_MR3 |  //
                    EXTI_INTENR_MR4 | EXTI_INTENR_MR5 | EXTI_INTENR_MR8 |                    //
                    EXTI_INTENR_MR13 | EXTI_INTENR_MR14 | EXTI_INTENR_MR15;                  // 割り込み有効化

    NVIC_EnableIRQ(EXTI7_0_IRQn);   // EXTI 7-0割り込みを有効にする
    NVIC_EnableIRQ(EXTI15_8_IRQn);  // EXTI 15-8割り込みを有効にする

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

volatile bool last_eject_signal_A = false;  // 前回のEJECT信号の状態
volatile bool last_eject_signal_B = false;  // 前回のEJECT信号の状態

void update_eject_signal(uint32_t porta) {
    bool eject = (porta & (1 << 4)) == 0;  // EJECT (Low=Eject)
    if ((porta & (1 << 2)) == 0) {         // OPTION_SELECT_A
        if (eject) {
            last_eject_signal_A = true;
        } else {
            last_eject_signal_A = false;
        }
    }
    if ((porta & (1 << 3)) == 0) {  // OPTION_SELECT_B
        if (eject) {
            last_eject_signal_B = true;
        } else {
            last_eject_signal_B = false;
        }
    }
}

/*
  EXTI 7-0 Global Interrupt Handler
 */
void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
void EXTI7_0_IRQHandler(void) {
    uint32_t porta = GPIOA->INDR;
    uint32_t intfr = EXTI->INTFR;  // 割り込みフラグを取得

    GPIOA->BSHR = (1 << 7);  // PA7 High (Buzzer, Debug)

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
        // PA2 (OPTION_SELECT_A) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF2;  // フラグをクリア
        if ((porta & (1 << 2)) != 0) {  // 立ち上がり
            // このタイミングで EJECT(PA4), EJECT_MASK(PA5), LED_BLINK(PA8)の状態を確認する
            drive_status_t* drive = &g_ctx->drive[0];  // Aドライブ
            if (last_eject_signal_A) {
                // 誤検出防止のために、事前にEJECT信号がアクティブになっていることを検知している時のみ有効とする
                pcfdd_force_eject(g_ctx, 0);  // Aドライブを強制排出
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
    if (intfr & EXTI_INTF_INTF3) {
        // PA3 (OPTION_SELECT_B) の割り込み
        EXTI->INTFR = EXTI_INTF_INTF3;  // フラグをクリア
        if ((porta & (1 << 3)) != 0) {  // 立ち上がり
            // このタイミングで EJECT(PA4), EJECT_MASK(PA5), LED_BLINK(PA8)の状態を確認する
            drive_status_t* drive = &g_ctx->drive[1];  // Bドライブ
            if (last_eject_signal_B) {
                // 誤検出防止のために、事前にEJECT信号がアクティブになっていることを検知している時のみ有効とする
                pcfdd_force_eject(g_ctx, 1);  // Bドライブを強制排出
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
    if (intfr & EXTI_INTF_INTF4) {
        EXTI->INTFR = EXTI_INTF_INTF4;  // フラグをクリア
        // PA4 (V2.0 EJECT) の割り込み
    }
    if (intfr & EXTI_INTF_INTF5) {
        EXTI->INTFR = EXTI_INTF_INTF5;  // フラグをクリア
        // PA7 (V2.0, EJECT_MASK) の割り込み
        // 特に何もしない
    }
    update_eject_signal(porta);
    GPIOA->BCR = (1 << 7);  // PA7 Low (Buzzer, Debug)
}

/*
  EXTI 15-8 Global Interrupt Handler
 */
void EXTI15_8_IRQHandler(void) __attribute__((interrupt));
void EXTI15_8_IRQHandler(void) {
    uint32_t intfr = EXTI->INTFR;  // 割り込みフラグを取得
    uint32_t porta = GPIOA->INDR;

    exti_int_counter++;

    if (intfr & EXTI_INTF_INTF8) {
        EXTI->INTFR = EXTI_INTF_INTF8;  // フラグをクリア
        // PA8 (V2.0, LED_BLINK) の割り込み
        // 特に何もしない
    }
    if (intfr & EXTI_INTF_INTF13) {
        EXTI->INTFR = EXTI_INTF_INTF13;  // フラグをクリア
                                         // PA13 (EJECT) の割り込み
        update_eject_signal(porta);
    }
    if (intfr & EXTI_INTF_INTF14) {
        EXTI->INTFR = EXTI_INTF_INTF14;  // フラグをクリア
        // PA14 (EJECT_MASK) の割り込み
        // 特に何もしない
    }
    if (intfr & EXTI_INTF_INTF15) {
        EXTI->INTFR = EXTI_INTF_INTF15;  // フラグをクリア
        // PA15 (LED_BLINK) の割り込み
        // 特に何もしない
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

    uint32_t PA = GPIOA->INDR;
    uint32_t PB = GPIOB->INDR;

    // EJECT信号のラッチ
    update_eject_signal(PA);

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