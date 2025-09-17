#include "pcfdd/pcfdd_control.h"

#include <stdbool.h>
#include <stdlib.h>

#include "ch32fun.h"
#include "oled/ssd1306_txt.h"

/**
 * @brief タイマーをタイムアウトさせるカウンタ値 -1
 * 128μ秒単位でカウントしているので、0x07ffだと 128μ秒 * 2048 = 262.144msec となる
 * この周期でタイムアウト割り込みがかかり、実際に 262msec 以上変化がないと判断した場合はタイムアウトとする
 * INDEXは 300RPMの時200msec, 360RPMの時166msec なので、262msec 以上へんかがなければ
 * INDEX信号が来ていないと判断できる
 */
const uint16_t TIMER_TIMEOUT = 0x07ff;

void pcfdd_init(void) {
    // PCFDDコントローラの初期化コードをここに追加

    //
    // FDのINDEX信号の立ち上がり/立ち下がりエッジを検出するために、Timer3 Channel1を使う
    //

    // リロードレジスタに最大値をセット(この値に達すると0に戻る)
    // TIMER_TIMEOUT * 128usec 周期ででタイムアウトするようにして、
    // 値の変化がない場合でも割り込み処理が動くようにしている
    TIM3->ATRLR = TIMER_TIMEOUT;
    // プリスケーラを設定
    // 48MHzのクロックを 48*128 で割ることで128µsの分解能にする (16bitカウンタで 8.38secまで数えられる)
    // 計測時に128倍の値を返すことで辻褄を合わせている
    TIM3->PSC = (48 * 128) - 1;  // 48MHz/(48*128) = 1/48μs * 48*128 -> 128µs resolution

    TIM3->SWEVGR |= TIM_UG;

    // Timer3 Channel 1を Capture/Compare の CC1に入力し、
    // 立ち下がりエッジを検出する
    // CC1の設定は TIM3->CHCTLR1 の下位8bitで設定できる
    // CC1 Select (CC1S) を 01にし、Channel 1を入力元とする
    TIM3->CHCTLR1 = TIM_CC1S_0;
    TIM3->CHCTLR1 |= TIM_IC1F_0 | TIM_IC1F_1;  // IC1F = 0b0110 (8サンプルのデジタルフィルタをかける)

    // CC1Eで、 CC1を有効にする
    // CC1P を1にすると、CC1は立ち下がりエッジを検出するようになる
    TIM3->CCER = TIM_CC1E | TIM_CC1P;

    // Timer3 のカウンタを有効にする, オートリロードモードを有効にする
    TIM3->CTLR1 = TIM_CEN | TIM_ARPE | TIM_URS;

    // Timer 3の割り込みを有効にする
    NVIC_EnableIRQ(TIM3_IRQn);

    // CC1IEで、 CC1の割り込みを有効にする
    // UIEで、アップデート割り込みを有効にする
    TIM3->DMAINTENR = TIM_CC1IE | TIM_UIE;

    //
    //
    //

    GPIOB->BSHR = (1 << 4);  // Motor ON for test
    // GPIOB->BSHR = (1 << 2);  // Drive Select A active for test
    GPIOB->BSHR = (1 << 3);  // Drive Select B active for test
}

/**
 * @brief キャプチャ割り込みで取得した回転数を保存する変数
 */
volatile uint32_t revolution = 0;

/*
  Timer3 Global Interrupt Handler
 */
void TIM3_IRQHandler(void) __attribute__((interrupt));
void TIM3_IRQHandler(void) {
    static uint16_t previousCapture = 0;  // 前回のキャプチャ値を保存する変数
    static uint32_t additional_count = 0;
    uint16_t currentCapture;
    bool overflowed = 0;

    // アップデート割り込み
    // カウンタの値が一周するタイミングで呼ばれるので、このタイミングで
    // 長期間値の変化が起きていないかどうかを検出します
    if (TIM3->INTFR & TIM_UIF) {
        TIM3->INTFR &= ~TIM_UIF;     // フラグをクリア
        currentCapture = TIM3->CNT;  // 通常は0のはず
        if (additional_count == 0) {
            // 初回は最後のキャプチャ値との差がタイムアウト値となる
            additional_count = (currentCapture - previousCapture) & TIMER_TIMEOUT;
        } else {
            // 2回目以降は1周の時間をたす
            additional_count += TIMER_TIMEOUT + 1;
        }
        previousCapture = TIM3->CNT;
        if (additional_count > 3906) {
            // debugprint(".");
            //  3906カウント(=128*3906=500msec) 以上値が変化していない場合はタイムアウトとみなす
            revolution = 0;
        } else {
            // debugprint("prev=%d, cur=%d, t=%ld\n", previousCapture, currentCapture, t);
        }
        // previousCapture = TIM2->CNT;
        return;
    }

    //  capture
    if (TIM3->INTFR & TIM_CC1IF) {
        // get capture
        currentCapture = TIM3->CH1CVR;
        TIM3->INTFR &= ~TIM_CC1IF;  // フラグをクリア
        // debugprint("CC1: %ld\n", currentCapture);
        // overflow
        if (TIM3->INTFR & TIM_CC1OF) {
            TIM3->INTFR &= ~TIM_CC1OF;  // フラグをクリア
            overflowed = 1;
        }

        // パルス間隔を保存
        // 128μ秒単位でカウントしているので、128倍する
        uint32_t diff = ((currentCapture - previousCapture) & TIMER_TIMEOUT) + additional_count;
        uint32_t width = (diff * 128) & 0xffffff;

        revolution = width;  // テスト
        additional_count = 0;
        previousCapture = currentCapture;  // 現在のキャプチャ値を保存
    }
}

uint32_t last_index_ms = 0;
bool last_index_state = false;

void pcfdd_poll(uint32_t systick_ms) {
    // PCFDDコントローラのポーリングコードをここに追加
    static uint64_t last_tick = 0;
    if (systick_ms - last_tick < 1000) {
        return;
    }
    last_tick = systick_ms;

    OLED_cursor(0, 4);
    OLED_printf("IDX %4dms ", revolution / 1000);  // usec -> msec

    // INDEX信号の読み取り
    /*    // PA6 : INDEX_DOSV (入力: INDEX信号, Pull-Up)
        if ((GPIOA->INDR & (1 << 6)) == 0) {
            // INDEX信号がLow(検出)のときの処理
            if (last_index_state == false) {
                // INDEX信号の立ち下がりを検出
                if (last_index_ms != 0) {
                    uint32_t period = systick_ms - last_index_ms;
                    // period msごとにINDEX信号が来ている
                    // 例: 200ms -> 5Hz, 100ms -> 10Hz, 50ms -> 20Hz
                    // ここで、periodに基づいて何かの処理を行うことができます。
                    OLED_cursor(0, 4);
                    OLED_printf("IDX %4dms ", period);
                }
                last_index_ms = systick_ms;
            }
            last_index_state = true;
        } else {
            last_index_state = false;
        }
        if (last_index_ms == 0) {
            OLED_cursor(0, 4);
            OLED_print("IDX ----ms ");
        }*/
}