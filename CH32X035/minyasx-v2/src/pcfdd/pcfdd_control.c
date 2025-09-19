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

#define BPS_TIM_PRESCALER_SHIFT 1  // 実際に取り込む頻度を下げる頻度 (IC1PSCの値。0=1/1, 1=1/2, 2=1/4, 3=1/8)
#define BPS_TIM_PRESCALER (1 << BPS_TIM_PRESCALER_SHIFT)

#define READ_DATA_CAP_N 512  // READ_DATAのキャプチャバッファのサイズ
volatile uint16_t cap_buf[READ_DATA_CAP_N];

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
    // READ_DATA の信号のエッジからbpsを測定するために、Timer1 Channel1を使う
    //

    TIM1->PSC = 12 - 1;    // 48MHz/(12) = 4MHz -> 0.25μs resolution
    TIM1->ATRLR = 0xffff;  // 16bitカウンタなので最大値をセット
    // Timer1 Channel 1を Capture/Compare の CC1に入力し、
    // 立ち下がりエッジを検出する
    // CC1の設定は TIM1->CHCTLR1 の下位8bitで設定できる
    // CC1 Select (CC1S) を 01にし、Channel 1を入力元とする
    // 入力キャプチャ: TI1, 分周なし, 軽いデジタルフィルタ
    TIM1->CHCTLR1 = TIM_CC1S_0;
    // TIM1->CHCTLR1 |= TIM_IC1F_0 | TIM_IC1F_1;  // IC1F = 0b0110 (8サンプルのデジタルフィルタをかける)
    TIM1->CHCTLR1 |= (BPS_TIM_PRESCALER_SHIFT << 2);  // IC1PSC = BPS_TIM_PRESCALER_SHIFT (DMA転送のプリスケーラを設定して頻度を下げる)

    // CC1Eで、 CC1を有効にする
    // CC1P を1にすると、CC1は立ち下がりエッジを検出するようになる
    TIM1->CCER = TIM_CC1E | TIM_CC1P;

    // CC1でDMA要求
    TIM1->DMAINTENR |= TIM_CC1DE;  // CC1IE
    // カウンタ有効化（= CR1.CEN）
    TIM1->CTLR1 |= TIM_CEN;

    // DMA1 Channel2を使って、Timer1 Channel1のキャプチャ値をバッファに保存する
    // DMA1_Channel2->PADDR = (uint32_t)&TIM1->CHCTLR1;
    DMA1_Channel2->PADDR = (uint32_t)&TIM1->CH1CVR;
    DMA1_Channel2->MADDR = (uint32_t)cap_buf;
    DMA1_Channel2->CNTR = READ_DATA_CAP_N;

    DMA1_Channel2->CFGR = (DMA_CFGR2_MEM2MEM * 0) |                      // MEM2MEM=0
                          (DMA_CFGR2_PL_1 * 0 | DMA_CFGR2_PL_0 * 0) |    // PL=low(必要ならup)
                          (DMA_CFGR2_MSIZE_1 * 0 | DMA_CFGR2_MSIZE_0) |  // MSIZE=16bit (01)
                          (DMA_CFGR2_PSIZE_1 * 0 | DMA_CFGR2_PSIZE_0) |  // PSIZE=16bit (01)
                          (DMA_CFGR2_MINC) |                             // MINC=1
                          (DMA_CFGR2_PINC * 0) |                         // PINC=0
                          (DMA_CFGR2_CIRC) |                             // CIRC=1
                          (DMA_CFGR2_DIR * 0) |                          // DIR=0 (P->M)
                          (DMA_CFGR2_TEIE) |                             // TEIE=1
                          (DMA_CFGR2_HTIE) |                             // HTIE=1
                          (DMA_CFGR2_TCIE) |                             // TCIE=1
                          (DMA_CFGR2_EN);                                // EN=1

    NVIC_EnableIRQ(DMA1_Channel2_IRQn);

    //
    //
    //

    GPIOB->BSHR = (1 << 4);  // Motor ON for test
    // GPIOB->BSHR = (1 << 2);  // Drive Select A active for test
    GPIOB->BSHR = (1 << 3);  // Drive Select B active for test
    // PB0: MODE_SELECT_DOSV
    GPIOB->BSHR = (1 << 0);  // 300RPM mode for test
}

/**
 * @brief キャプチャ割り込みで取得した回転数を保存する変数
 */
volatile uint32_t revolution = 0;
volatile uint32_t index_width = 0;  // INDEXの幅（msec単位）

/*
  Timer3 Global Interrupt Handler
 */
void TIM3_IRQHandler(void) __attribute__((interrupt));
void TIM3_IRQHandler(void) {
    static uint16_t previousCapture = 0;  // 前回のキャプチャ値を保存する変数
    static uint32_t additional_count = 0;
    uint16_t currentCapture;

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
            index_width = 0;
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
        }

        // パルス間隔を保存
        // 128μ秒単位でカウントしているので、128倍する
        uint32_t diff = ((currentCapture - previousCapture) & TIMER_TIMEOUT) + additional_count;
        uint32_t width = (diff * 128) & 0xffffff;

        index_width = width / 1000;  // usec -> msec

        additional_count = 0;
        previousCapture = currentCapture;  // 現在のキャプチャ値を保存
    }
}

/**
 * READ DATA の bps計測（片エッジ/整数）
 * 前提:
 *   - タイマtick = 0.25us (例: 48MHz/PSC=11 → 4MHz)
 *   - 観測BPS = 記録BPS × (読出RPM / 記録RPM)
 * 代表的な最短セル幅(≒最小パルス間隔)とtick値:
 *   ~600 kbps → 1.667us ≈ 6.7tick  → "7ish"
 *   ~500 kbps → 2.000us =  8tick   → "8ish"
 *   ~416.7kbps→ 2.400us ≈ 9.6tick  → "10ish"
 *   ~300 kbps → 3.333us ≈ 13.3tick → "13ish"
 *   ~250 kbps → 4.000us =  16tick  → "16ish"
 *
 * 窓（ジッタ考慮の目安。環境で±1調整推奨）:
 *   7ish : [6..7]        (≈1.5..1.75us)
 *   8ish : [8..9]        (2.0..2.25us)
 *   10ish: [9..11]       (2.25..2.75us)
 *   13ish: [13..14]      (3.25..3.5us)
 *   16ish: [15..17]      (3.75..4.25us)
 * それ以外は cnt_other へ。
 */

static volatile uint16_t prev_ccr;
static volatile uint32_t cnt_7ish, cnt_8ish, cnt_10ish, cnt_13ish, cnt_16ish, cnt_other;
static volatile uint32_t dma_int_count;

// 窓しきい値（必要ならここだけ変えれば挙動を調整可能）
#define WIN_7_MIN (6 * BPS_TIM_PRESCALER)
#define WIN_8_MIN (8 * BPS_TIM_PRESCALER)
#define WIN_10_MIN (10 * BPS_TIM_PRESCALER)
#define WIN_13_MIN (13 * BPS_TIM_PRESCALER)
#define WIN_16_MIN (16 * BPS_TIM_PRESCALER)
#define WIN_7_MAX (WIN_8_MIN - 1)
#define WIN_8_MAX (WIN_10_MIN - 1)
#define WIN_10_MAX (WIN_13_MIN - 1)
#define WIN_13_MAX (WIN_16_MIN - 1)
#define WIN_16_MAX (17 * BPS_TIM_PRESCALER)

// 粗ノイズ除去（0.5us未満や6us超は即捨て: 2tick=0.5us, 24tick=6us）
#define DT_MIN_TICKS (2 * BPS_TIM_PRESCALER)
#define DT_MAX_TICKS (24 * BPS_TIM_PRESCALER)

uint16_t last_dt;

static inline void classify_dt(uint16_t dt) {
    last_dt = dt;
    // 粗ノイズ除去
    if (dt < DT_MIN_TICKS || dt > DT_MAX_TICKS) {
        cnt_other++;
        return;
    }

    // 分岐コストを抑えるための“範囲内チェック”は (uint8_t)(dt - MIN) <= (MAX - MIN) も可
    if ((uint8_t)(dt - WIN_7_MIN) <= (WIN_7_MAX - WIN_7_MIN)) {
        cnt_7ish++;
        return;
    }  // ~1.67us (≈600k)
    if ((uint8_t)(dt - WIN_8_MIN) <= (WIN_8_MAX - WIN_8_MIN)) {
        cnt_8ish++;
        return;
    }  // ~2.00us (500k)
    if ((uint8_t)(dt - WIN_10_MIN) <= (WIN_10_MAX - WIN_10_MIN)) {
        cnt_10ish++;
        return;
    }  // ~2.40us (≈416.7k)
    if ((uint8_t)(dt - WIN_13_MIN) <= (WIN_13_MAX - WIN_13_MIN)) {
        cnt_13ish++;
        return;
    }  // ~3.33us (≈300k)
    if ((uint8_t)(dt - WIN_16_MIN) <= (WIN_16_MAX - WIN_16_MIN)) {
        cnt_16ish++;
        return;
    }  // ~4.00us (250k)

    cnt_other++;  // 窓外
}

static void process_block(const volatile uint16_t* blk, size_t n) {
    if ((dma_int_count & 0x1F) != 0) {
        // 負荷軽減のため、32回に1回だけ処理する
        return;
    }

    uint16_t p = blk[0];
    for (size_t i = 1; i < n; i++) {
        uint16_t c = blk[i];
        uint16_t dt = (uint16_t)(c - p);
        p = c;
        // グリッチ粗除去（0.25〜5us以外はバッサリ）
        //        if (dt < 2 || dt > 20) continue;  // 0.5us 未満 or 5us 超は無視
        classify_dt(dt);
    }
    prev_ccr = p;
}

/*
  DMA1 Channel2 Global Interrupt Handler
 */
void DMA1_Channel2_IRQHandler(void) __attribute__((interrupt));
void DMA1_Channel2_IRQHandler(void) {
    dma_int_count++;

    // DMA1 Channel2の割り込み処理をここに追加
    uint32_t isr = DMA1->INTFR;  // Interrupt Flag Registerの値を取得

    if (isr & DMA_HTIF2) {          // Half Transfer Interrupt Flag
        DMA1->INTFCR = DMA_CHTIF2;  // フラグをクリア
        process_block(&cap_buf[0], READ_DATA_CAP_N / 2);
    }
    if (isr & DMA_TCIF2) {          // Transfer Complete Interrupt Flag
        DMA1->INTFCR = DMA_CTCIF2;  // フラグをクリア
        process_block(&cap_buf[READ_DATA_CAP_N / 2], READ_DATA_CAP_N / 2);
    }
    if (isr & DMA_TEIF2) {          // Transfer Error Interrupt Flag
        DMA1->INTFCR = DMA_CTEIF2;  // フラグをクリア
        // 必要なら再初期化
    }

    // まれにCC1OF対策でINTFR/CVRを読んでフラグ掃除
    volatile uint32_t sr = TIM1->INTFR;
    (void)sr;  // 未使用警告回避
    volatile uint32_t junk = TIM1->CH1CVR;
    (void)junk;  // 未使用警告回避
}

// 観測しうるBPSカテゴリ
//  - 600k : (500k @300rpm) を 360rpmで読んだ等 → 最短 ~1.67us ≈ 7tick
//  - 500k : 1.2M/1.44M 回転一致 → 最短 2.0us = 8tick
//  - 416k : (500k @360rpm) を 300rpmで読んだ → 最短 ~2.4us ≈ 10tick
//  - 300k : (250k @300rpm) を 360rpmで読んだ → 最短 ~3.33us ≈ 13tick
//  - 250k : 2DD 回転一致 → 最短 4.0us = 16tick
typedef enum {
    BPS_UNKNOWN,
    BPS_250K,
    BPS_300K,
    BPS_416K,
    BPS_500K,
    BPS_600K,
} bps_mode_t;

// しきい値（必要に応じて調整）
#define VOTES_MIN 50          // 有効票の最小数（ギャップや無信号を弾く）
#define TOP_THRESHOLD_PCT 20  // 最多ビンが全体の何%以上なら採用するか

// 既存の分類カウンタ（classify_dt() で増やしているもの）
extern volatile uint32_t cnt_7ish, cnt_8ish, cnt_10ish, cnt_13ish, cnt_16ish;
extern volatile uint32_t cnt_other;

// 必要なら数値にしたい時用（表示など）
static inline uint32_t bps_mode_to_value(bps_mode_t m) {
    switch (m) {
    case BPS_600K:
        return 600000;
    case BPS_500K:
        return 500000;
    case BPS_416K:
        return 416000;  // 416,667に丸めたければ 416667
    case BPS_300K:
        return 300000;
    case BPS_250K:
        return 250000;
    default:
        return 0;
    }
}

static inline bps_mode_t bin_index_to_mode(int idx) {
    // idx: 0=7ish, 1=8ish, 2=10ish, 3=13ish, 4=16ish
    switch (idx) {
    case 0:
        return BPS_600K;
    case 1:
        return BPS_500K;
    case 2:
        return BPS_416K;
    case 3:
        return BPS_300K;
    case 4:
        return BPS_250K;
    default:
        return BPS_UNKNOWN;
    }
}

// 1秒ごとなどに呼ぶ想定：最多ビンの多数決でBPSを返し、カウンタをゼロクリア
bps_mode_t decide_and_reset(void) {
    // スナップショット
    uint32_t c7 = cnt_7ish;
    uint32_t c8 = cnt_8ish;
    uint32_t c10 = cnt_10ish;
    uint32_t c13 = cnt_13ish;
    uint32_t c16 = cnt_16ish;

    // クリア（次の集計へ）
    cnt_7ish = cnt_8ish = cnt_10ish = cnt_13ish = cnt_16ish = 0;
    cnt_other = 0;

    uint32_t votes = c7 + c8 + c10 + c13 + c16;
    if (votes < VOTES_MIN) {
        //     return BPS_UNKNOWN;
    }

    // 最多ビン（単純多数決）
    uint32_t bins[5] = {c7, c8, c10, c13, c16};
    int top_idx = 0;
    for (int i = 1; i < 5; i++) {
        if (bins[i] > bins[top_idx]) top_idx = i;
    }
    uint32_t top = bins[top_idx];

    // 最低比率チェック（55%など）。環境により 50〜60% で調整。
    if (top * 100U < votes * TOP_THRESHOLD_PCT) {
        return BPS_UNKNOWN;  // 票が割れている/信号が不安定
    }

    return bin_index_to_mode(top_idx);
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
    if (index_width > 166 - 5 && index_width < 166 + 5) {
        revolution = 360;
        OLED_printf("REV:%3drpm (%3dms)", revolution, (int)index_width);
    } else if (index_width > 200 - 5 && index_width < 200 + 5) {
        revolution = 300;
        OLED_printf("REV:%3drpm (IDX:%3dms)", revolution, (int)index_width);
    } else {
        revolution = 0;
        OLED_printf("REV:---rpm", revolution, (int)index_width);
    }

    OLED_cursor(0, 5);
#ifdef DEBUG
    OLED_printf("7:%d 8:%d 10:%d 13:%d 16:%d dt:%d\n",                                                                  //
                (int)cnt_7ish / 10, (int)cnt_8ish / 10, (int)cnt_10ish / 10, (int)cnt_13ish / 10, (int)cnt_16ish / 10,  //
                (int)last_dt);
#endif
    bps_mode_t bps = decide_and_reset();
    switch (bps) {
    case BPS_250K:
        OLED_print("BPS:250K");
        break;
    case BPS_300K:
        OLED_print("BPS:300K");
        break;
    case BPS_416K:
        OLED_print("BPS:416K");
        break;
    case BPS_500K:
        OLED_print("BPS:500K");
        break;
    case BPS_600K:
        OLED_print("BPS:600K");
        break;
    default:
        OLED_print("BPS:----");
        break;
    }

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