#include "pcfdd/pcfdd_control.h"

#include <stdbool.h>
#include <stdlib.h>

#include "ch32fun.h"
#include "ui/ui_control.h"

/**
 * @brief タイマーをタイムアウトさせるカウンタ値 -1
 * 128μ秒単位でカウントしているので、0x07ffだと 128μ秒 * 2048 = 262.144msec となる
 * この周期でタイムアウト割り込みがかかり、実際に 262msec 以上変化がないと判断した場合はタイムアウトとする
 * INDEXは 300RPMの時200msec, 360RPMの時166msec なので、262msec 以上へんかがなければ
 * INDEX信号が来ていないと判断できる
 */
const uint16_t TIMER_TIMEOUT = 0x07ff;

#define BPS_TIM_PRESCALER_SHIFT 1                         // 実際に取り込む頻度を下げる頻度 (IC1PSCの値。0=1/1, 1=1/2, 2=1/4, 3=1/8)
#define BPS_TIM_PRESCALER (1 << BPS_TIM_PRESCALER_SHIFT)  // 48MHz/(BPS_TIM_PRESCALER) = 4MHz(既定) → 0.25us resolution

#define SCALE (1u << BPS_TIM_PRESCALER_SHIFT)
#define S_LO(x) ((x) * SCALE)
#define S_HI(x) ((x) * SCALE + (SCALE - 1))  // 上側を「その刻みの最終tick」まで広げる

/* --- ドライブ別 DMA バッファ（CCR=CH1CVRは32bit。まずは32bit受け推奨） --- */
#define READ_DATA_CAP_N 1024
static volatile uint32_t cap_buf_ds0[READ_DATA_CAP_N];
static volatile uint32_t cap_buf_ds1[READ_DATA_CAP_N];
static volatile uint32_t* cap_buf_active = cap_buf_ds0; /* 切替用 */

void set_mode_select(drive_status_t* drive, fdd_rpm_mode_t rpm) {
    uint32_t flag = (1 << 0);  // MODE_SELECT_DOSV のビット位置
    bool inverted = drive->mode_select_inverted;
    drive->rpm_setting = rpm;
    // MODE_SELECT_DOSV の設定
    if (((rpm == FDD_RPM_300) && !inverted) || ((rpm == FDD_RPM_360) && inverted)) {
        GPIOB->BCR = flag;  // MODE_SELECT_DOSV = 300RPM mode
    } else {
        GPIOB->BSHR = flag;  // MODE_SELECT_DOSV = 360RPM mode
    }
}

// ---- 設定に応じて調整する定数 ----
#define TIMEOUT_US (500000u)  // 500ms
#define UIF_TICK_US (1000u)   // UIF 1ms

// ---- 測定結果：msec 単位（従来踏襲）----
volatile uint32_t index_width[2] = {0, 0};  // [A,B]

// ---- SysTick ベース内部状態 ----
static volatile uint32_t s_last_edge_cycles = 0;  // 前回エッジ SysTick(48MHz)
static volatile uint8_t s_have_prev_edge = 0;     // 初回保護

typedef enum { DRIVE_NONE = -1, DRIVE_A = 0, DRIVE_B = 1 } drive_t;
static volatile drive_t s_current_drive = DRIVE_NONE;

static inline drive_t current_drive_from_gpio(void) {
    const bool ds_a = (GPIOB->INDR & (1 << 2)) != 0;
    const bool ds_b = (GPIOB->INDR & (1 << 3)) != 0;
    if (ds_a && !ds_b) return DRIVE_A;
    if (!ds_a && ds_b) return DRIVE_B;
    // 想定外（両方0 or 両方1）は、前回を維持せず NONE とする
    return DRIVE_NONE;
}

void pcfdd_init(minyasx_context_t* ctx) {
    // PCFDDコントローラの初期化コードをここに追加
    for (int i = 0; i < 2; i++) {
        ctx->drive[i].connected = true;  // TODO
        ctx->drive[i].force_ejected = false;
        ctx->drive[i].eject_masked = false;
        ctx->drive[i].inserted = false;
        ctx->drive[i].ready = false;
        ctx->drive[i].led_blink = false;
        ctx->drive[i].rpm_control = FDD_RPM_CONTROL_9SCDRV;
        ctx->drive[i].rpm_setting = FDD_RPM_360;
        ctx->drive[i].rpm_measured = FDD_RPM_UNKNOWN;
        ctx->drive[i].bps_measured = BPS_UNKNOWN;
    }
    //
    // FDのINDEX信号の立ち上がり/立ち下がりエッジを検出するために、Timer3 Channel1を使う
    // Timer3は Channel2で BeepのPWM出力でも使っていて、動的にタイムアウト値(ARR)が変更されてしまうので、
    // Indexパルス幅の検出は、SysTick->CNTの値を直接読むことで実現している
    //

    // プリスケーラを設定
    // 48MHzのクロックを 48 で割ることで1µsの分解能にする
    TIM3->PSC = 48 - 1;  // 48MHz/48 = 1/48μs * 48 -> 1µs resolution
    // リロードレジスタはBeepのPWM出力で使うので、ここでは設定しない
    // TIM3->ATRLR = 0;
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
    // Timer1 Channel 1を Capture/Compare の CC1に入力
    // CC1 Select (CC1S) を 01にし、Channel 1を入力元とする
    // 入力キャプチャ: TI1, 分周なし, 軽いデジタルフィルタ
    TIM1->CHCTLR1 = TIM_CC1S_0;
    TIM1->CHCTLR1 |= (BPS_TIM_PRESCALER_SHIFT << 2);  // IC1PSC = BPS_TIM_PRESCALER_SHIFT (DMA転送のプリスケーラを設定して頻度を下げる)
    // TIM1->CHCTLR1 |= TIM_IC1F_0 | TIM_IC1F_1;  // 必要ならデジタルフィルタ

    // CC1Eで、 CC1を有効にする（※起動はDS選択時に行う）
    // CC1P を1にすると、CC1は立ち下がりエッジを検出するようになる
    TIM1->CCER = /*TIM_CC1E |*/ TIM_CC1P;

    /* CC DMA は CCイベントで出す（重要。UEVではなくCCで発火） */
    TIM1->CTLR2 &= ~TIM_CCDS;

    /* DMA1 Channel2を使って、Timer1 Channel1のキャプチャ値をバッファに保存する。
       CCR1は CH1CVR(32bit) にラッチされるので、PSIZE/MSIZE=32bit で受ける */
    DMA1_Channel2->PADDR = (uint32_t)&TIM1->CH1CVR;  // ★ CH1CVR を読む（従来 CHCTLR1 だったのを修正）
    DMA1_Channel2->MADDR = (uint32_t)cap_buf_active;
    DMA1_Channel2->CNTR = READ_DATA_CAP_N;

    DMA1_Channel2->CFGR = (0 * DMA_CFGR2_DIR) /* DIR=0: P->M */
                          | DMA_CFGR2_CIRC    /* CIRC=1       */
                          | DMA_CFGR2_MINC    /* MINC=1       */
                          | DMA_CFGR2_PSIZE_1 /* PSIZE=10b: 32-bit */
                          | DMA_CFGR2_MSIZE_1 /* MSIZE=10b: 32-bit */
                          | DMA_CFGR2_HTIE    /* Half */
                          | DMA_CFGR2_TCIE    /* Full */
        /* | DMA_CFGR2_PL_1 */;               /* 必要に応じて優先度を上げる */

    // まだ開始しない（DSが来たら開始）:
    TIM1->DMAINTENR &= ~TIM_CC1DE;        // CC1 DMA要求停止
    DMA1_Channel2->CFGR &= ~DMA_CFGR2_EN; /* DMA停止 */
    TIM1->CCER &= ~TIM_CC1E;              // 入力キャプチャ停止

    // カウンタ自体は回しておく（どちらでも良い）
    TIM1->CTLR1 |= TIM_CEN;

    // DMA割り込み有効
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);

    //
    // シーク
    //
    int seek_count = 0;
    // シークA
    GPIOB->BSHR = (1 << 2);  // Drive Select A active for test
    GPIOB->BSHR = (1 << 5);  // DIRECTION_DOSV active (内周方向)
    for (int i = 0; i < 50; i++) {
        GPIOB->BSHR = (1 << 6);  // STEP_DOSV = 1
        Delay_Ms(1);
        GPIOB->BCR = (1 << 6);  // STEP_DOSV = 0
        Delay_Ms(3);
    }
    GPIOB->BCR = (1 << 5);  // DIRECTION_DOSV inactive (正論理, 外周方向)
    seek_count = 0;
    while (seek_count < 200) {
        seek_count++;
        // TRACK0を見て、トラック0に到達したら抜ける
        if ((GPIOB->INDR & (1 << 10)) == 0) {
            // TRACK0_DOSV = 0 (Low) になった
            break;
        }
        GPIOB->BSHR = (1 << 6);  // STEP_DOSV = 1
        Delay_Ms(1);
        GPIOB->BCR = (1 << 6);  // STEP_DOSV = 0
        Delay_Ms(3);
    }
    GPIOB->BCR = (1 << 2);  // Drive Select A inactive for test

    Delay_Ms(1);

    // シークB
    GPIOB->BSHR = (1 << 3);  // Drive Select B active for test
    GPIOB->BSHR = (1 << 5);  // DIRECTION_DOSV active (内周方向)
    for (int i = 0; i < 50; i++) {
        GPIOB->BSHR = (1 << 6);  // STEP_DOSV = 1
        Delay_Ms(1);
        GPIOB->BCR = (1 << 6);  // STEP_DOSV = 0
        Delay_Ms(3);
    }
    GPIOB->BCR = (1 << 5);  // DIRECTION_DOSV inactive (正論理, 外周方向)
    seek_count = 0;
    while (seek_count < 200) {
        seek_count++;
        // TRACK0を見て、トラック0に到達したら抜ける
        if ((GPIOB->INDR & (1 << 10)) == 0) {
            // TRACK0_DOSV = 0 (Low) になった
            break;
        }
        GPIOB->BSHR = (1 << 6);  // STEP_DOSV = 1
        Delay_Ms(1);
        GPIOB->BCR = (1 << 6);  // STEP_DOSV = 0
        Delay_Ms(3);
    }
    GPIOB->BCR = (1 << 3);  // Drive Select B inactive for test

    //
    set_mode_select(&ctx->drive[0], ctx->drive[0].rpm_setting);
    set_mode_select(&ctx->drive[1], ctx->drive[1].rpm_setting);
}

/*
  Timer3 IRQ: CC1IF -> エッジ検出（SysTick で幅計算）
              UIF   -> タイムアウト監視（現在の対象ドライブに対して）
 */
void TIM3_IRQHandler(void) __attribute__((interrupt));
void TIM3_IRQHandler(void) {
    drive_t drv = current_drive_from_gpio();
    // ---- UIF: タイムアウト監視（現在の対象ドライブに限定）----
    if (TIM3->INTFR & TIM_UIF) {
        TIM3->INTFR &= ~TIM_UIF;

        if (drv != DRIVE_NONE && s_current_drive == drv && s_have_prev_edge) {
            uint32_t now_cycles = SysTick->CNT;
            uint32_t delta_us = (now_cycles - s_last_edge_cycles) / 48u;
            if (delta_us > TIMEOUT_US) {
                index_width[drv] = 0;  // 500ms 以上エッジ無し → タイムアウト
                // 基準は保持（次のエッジで復帰）
            }
        }
        return;
    }

    // ---- CC1IF: エッジ到来 ----
    if (TIM3->INTFR & TIM_CC1IF) {
        (void)TIM3->CH1CVR;  // 読み出し（値は使わない）
        TIM3->INTFR &= ~TIM_CC1IF;
        if (TIM3->INTFR & TIM_CC1OF) {
            TIM3->INTFR &= ~TIM_CC1OF;
        }

        // どちらのドライブのエッジか判定
        // 対象が切り替わったら“やり直し”
        if (drv != s_current_drive) {
            s_current_drive = drv;
            s_have_prev_edge = 0;
            s_last_edge_cycles = 0;
        }

        // NONE（不明）なら今回のエッジは無視
        if (drv == DRIVE_NONE) return;

        uint32_t now_cycles = SysTick->CNT;

        if (!s_have_prev_edge) {
            // 基準確立のみ
            s_last_edge_cycles = now_cycles;
            s_have_prev_edge = 1;
            return;
        }

        // 幅（µs）= 48MHz 差分 / 48 （四捨五入のために +24）
        uint32_t width_us = (now_cycles - s_last_edge_cycles + 24u) / 48u;
        uint32_t width_ms = width_us / 1000u;

        index_width[drv] = width_ms;

        // 基準更新
        s_last_edge_cycles = now_cycles;
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

/* --- ドライブ別統計 --- */
typedef struct {
    volatile uint16_t prev_ccr;
    volatile uint8_t prev_valid;
    volatile uint32_t cnt_7ish, cnt_8ish, cnt_10ish, cnt_13ish, cnt_16ish, cnt_other;
} rd_stats_t;

static volatile rd_stats_t g_stats[2] = {0};

/* アクティブドライブ: 0=DS0, 1=DS1, 0xFF=なし（停止中） */
static volatile uint8_t g_active = 0xFF;

/* last_dt など既存デバッグ変数は存続 */
static volatile uint32_t dma_int_count;
uint16_t last_dt;

/* --- しきい値（スケーリング S() 適用）--- */
#define WIN_7_MIN S_LO(6) /* ~1.5..1.75us  ≈600k */
#define WIN_7_MAX S_HI(7)
#define WIN_8_MIN S_LO(8) /* 2.0..2.25us   500k  */
#define WIN_8_MAX S_HI(9)
#define WIN_10_MIN S_LO(9) /* 2.25..2.75us ≈416.7k */
#define WIN_10_MAX S_HI(11)
#define WIN_13_MIN S_LO(13) /* 3.25..3.5us  ≈300k */
#define WIN_13_MAX S_HI(14)
#define WIN_16_MIN S_LO(15) /* 3.75..4.25us  250k  */
#define WIN_16_MAX S_HI(17)

/* 粗ノイズ除去もしきい値を同様に（下限はS_LO、上限はS_HI） */
#define DT_MIN_TICKS S_LO(2)
#define DT_MAX_TICKS S_HI(24)

/* --- 既存API互換: bps列挙→数値 --- */
uint32_t pcfdd_bps_value(fdd_bps_mode_t m) {
    switch (m) {
    case BPS_600K:
        return 600000;
    case BPS_500K:
        return 500000;
    case BPS_416K:
        return 416000; /* 必要なら 416667 */
    case BPS_300K:
        return 300000;
    case BPS_250K:
        return 250000;
    default:
        return 0;
    }
}

static void capture_pause(void) {
    TIM1->CCER &= ~TIM_CC1E;               // CCR更新止める
    TIM1->DMAINTENR &= ~TIM_CC1DE;         // DMA要求止める
    DMA1_Channel2->CFGR &= ~DMA_CFGR2_EN;  // DMA停止
    DMA1->INTFCR = DMA_CHTIF2 | DMA_CTCIF2 | DMA_CTEIF2;
    (void)TIM1->INTFR;
    (void)TIM1->CH1CVR;  // SR→CCR 読み捨てでフラグ掃除
    g_active = 0xFF;
}

static void capture_start_for_drive(uint8_t d) {
    cap_buf_active = (d == 0) ? cap_buf_ds0 : cap_buf_ds1;

    /* バッファ再装填 */
    DMA1_Channel2->MADDR = (uint32_t)cap_buf_active;
    DMA1_Channel2->CNTR = READ_DATA_CAP_N;
    DMA1->INTFCR = DMA_CHTIF2 | DMA_CTCIF2 | DMA_CTEIF2;
    DMA1_Channel2->CFGR |= DMA_CFGR2_EN;

    /* 再開直後の巨大dtを捨てる */
    g_stats[d].prev_ccr = (uint16_t)(TIM1->CH1CVR & 0xFFFF);
    g_stats[d].prev_valid = 0;

    TIM1->DMAINTENR |= TIM_CC1DE;  // CC1 DMA要求開始
    TIM1->CCER |= TIM_CC1E;        // 入力キャプチャ開始

    g_active = d;
}

static inline void classify_dt(uint16_t dt) {
    last_dt = dt;
    // 粗ノイズ除去
    if (dt < DT_MIN_TICKS || dt > DT_MAX_TICKS) {
        if (g_active <= 1) g_stats[g_active].cnt_other++;
        return;
    }

    /* 分岐コストを抑えるための“範囲内チェック”は (uint8_t)(dt - MIN) <= (MAX - MIN) も可 */
    if ((uint8_t)(dt - WIN_7_MIN) <= (WIN_7_MAX - WIN_7_MIN)) {
        if (g_active <= 1) g_stats[g_active].cnt_7ish++;
        return;  // ~1.67us (≈600k)
    }
    if ((uint8_t)(dt - WIN_8_MIN) <= (WIN_8_MAX - WIN_8_MIN)) {
        if (g_active <= 1) g_stats[g_active].cnt_8ish++;
        return;  // ~2.00us (500k)
    }
    if ((uint8_t)(dt - WIN_10_MIN) <= (WIN_10_MAX - WIN_10_MIN)) {
        if (g_active <= 1) g_stats[g_active].cnt_10ish++;
        return;  // ~2.40us (≈416.7k)
    }
    if ((uint8_t)(dt - WIN_13_MIN) <= (WIN_13_MAX - WIN_13_MIN)) {
        if (g_active <= 1) g_stats[g_active].cnt_13ish++;
        return;  // ~3.33us (≈300k)
    }
    if ((uint8_t)(dt - WIN_16_MIN) <= (WIN_16_MAX - WIN_16_MIN)) {
        if (g_active <= 1) g_stats[g_active].cnt_16ish++;
        return;  // ~4.00us (250k)
    }

    if (g_active <= 1) g_stats[g_active].cnt_other++;  // 窓外
}

/* 32bit受け（CH1CVR）の下位16bitを差分化 */
static void process_block32(const volatile uint32_t* blk, size_t n) {
    if (g_active > 1) return;

    uint16_t p = g_stats[g_active].prev_ccr;
    for (size_t i = 0; i < n; i++) {
        uint16_t c = (uint16_t)(blk[i] & 0xFFFF);
        if (!g_stats[g_active].prev_valid) {
            g_stats[g_active].prev_ccr = c;
            g_stats[g_active].prev_valid = 1;
            p = c;
            continue;
        }
        uint16_t dt = (uint16_t)(c - p);
        p = c;
        classify_dt(dt);
    }
    g_stats[g_active].prev_ccr = p;
}

/*
  DMA1 Channel2 Global Interrupt Handler
 */
void DMA1_Channel2_IRQHandler(void) __attribute__((interrupt));
void DMA1_Channel2_IRQHandler(void) {
    dma_int_count++;
    uint32_t isr = DMA1->INTFR;

    if (isr & DMA_HTIF2) {
        DMA1->INTFCR = DMA_CHTIF2;
        process_block32(&cap_buf_active[0], READ_DATA_CAP_N / 2);
    }
    if (isr & DMA_TCIF2) {
        DMA1->INTFCR = DMA_CTCIF2;
        process_block32(&cap_buf_active[READ_DATA_CAP_N / 2], READ_DATA_CAP_N / 2);
    }
    if (isr & DMA_TEIF2) {
        DMA1->INTFCR = DMA_CTEIF2;
        // 必要なら再初期化
    }

    // まれにCC1OF対策でINTFR/CVRを読んでフラグ掃除
    (void)TIM1->INTFR;
    (void)TIM1->CH1CVR;
}

// 既存の分類カウンタ（classify_dt() で増やしているもの）
extern volatile uint32_t cnt_7ish, cnt_8ish, cnt_10ish, cnt_13ish, cnt_16ish;
extern volatile uint32_t cnt_other;

// 必要なら数値にしたい時用（表示など）
inline uint32_t fdd_bps_mode_to_value(fdd_bps_mode_t m) {
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

static inline fdd_bps_mode_t bin_index_to_mode(int idx) {
    switch (idx) {
    case 0:
        return BPS_600K;  // 7ish
    case 1:
        return BPS_500K;  // 8ish
    case 2:
        return BPS_416K;  // 10ish
    case 3:
        return BPS_300K;  // 13ish
    case 4:
        return BPS_250K;  // 16ish
    default:
        return BPS_UNKNOWN;
    }
}

#define VOTES_MIN 50         /* 有効票の最小数（ギャップや無信号を弾く） */
#define TOP_THRESHOLD_PCT 20 /* 最多ビンの下限比率 */

fdd_bps_mode_t pcfdd_bps_decide_and_reset(int drive) {
    if (drive < 0 || drive > 1) return BPS_UNKNOWN;
    rd_stats_t* S = (rd_stats_t*)&g_stats[drive];

    uint32_t c7 = S->cnt_7ish, c8 = S->cnt_8ish, c10 = S->cnt_10ish, c13 = S->cnt_13ish, c16 = S->cnt_16ish;
    S->cnt_7ish = S->cnt_8ish = S->cnt_10ish = S->cnt_13ish = S->cnt_16ish = S->cnt_other = 0;

    uint32_t votes = c7 + c8 + c10 + c13 + c16;
    ui_cursor(UI_PAGE_DEBUG_PCFDD, 0, 3 + drive * 2);
    ui_printf(UI_PAGE_DEBUG_PCFDD, "%d:%d:%d:%d:%d[%d:%d]\n",  //
              (int)c7 / 10, (int)c8 / 10, (int)c10 / 10, (int)c13 / 10, (int)c16 / 10, (int)S->cnt_other / 10, (int)votes / 10);
    if (votes < VOTES_MIN) return BPS_UNKNOWN;

    uint32_t bins[5] = {c7, c8, c10, c13, c16};
    int top = 0;
    for (int i = 1; i < 5; i++)
        if (bins[i] > bins[top]) top = i;

    if (bins[top] * 100U < votes * TOP_THRESHOLD_PCT) return BPS_UNKNOWN;
    return bin_index_to_mode(top);
}

//
//

static volatile pcfdd_ds_t g_current_ds = PCFDD_DS_NONE;

/* 別モジュールから現在のDRIVE_SELECT状態を通知する */
void pcfdd_set_current_ds(pcfdd_ds_t ds) {
    if (ds == g_current_ds) return;

    /* 一旦止めてから必要なら再開。DS0/DS1→0/1 にマップ */
    capture_pause();
    if (ds == PCFDD_DS0)
        capture_start_for_drive(0);
    else if (ds == PCFDD_DS1)
        capture_start_for_drive(1);

    g_current_ds = ds;
}

//
//
//

uint32_t last_index_ms = 0;
bool last_index_state = false;

void pcfdd_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // PCFDDコントローラのポーリングコードをここに追加
    static uint64_t last_tick = 0;
    if (systick_ms - last_tick < 1000) {
        return;
    }
    last_tick = systick_ms;

    ui_cursor(UI_PAGE_DEBUG_PCFDD, 0, 0);
    for (int i = 0; i < 2; i++) {
        if (index_width[i] > 166 - 5 && index_width[i] < 166 + 5) {
            ctx->drive[i].rpm_measured = FDD_RPM_360;
            ui_printf(UI_PAGE_DEBUG_PCFDD, "REV:360  ");
        } else if (index_width[i] > 200 - 5 && index_width[i] < 200 + 5) {
            ctx->drive[i].rpm_measured = FDD_RPM_300;
            ui_printf(UI_PAGE_DEBUG_PCFDD, "REV:300  ");
        } else {
            ctx->drive[i].rpm_measured = FDD_RPM_UNKNOWN;
            ui_printf(UI_PAGE_DEBUG_PCFDD, "REV:---- ");
        }
    }

    fdd_bps_mode_t bps0 = pcfdd_bps_decide_and_reset(0); /* DS0のbpsを取得 */
    fdd_bps_mode_t bps1 = pcfdd_bps_decide_and_reset(1); /* DS1のbpsを取得 */
    ctx->drive[0].bps_measured = bps0;
    ctx->drive[1].bps_measured = bps1;

#if 0
    ui_cursor(UI_PAGE_DEBUG_PCFDD, 0, 4);
    ui_printf(UI_PAGE_DEBUG_PCFDD, "BPS:%3dk BPS:%3dk", fdd_bps_mode_to_value(bps0) / 1000, fdd_bps_mode_to_value(bps1) / 1000);
#endif
}

void pcfdd_update_setting(minyasx_context_t* ctx, int drive) {
    if (drive < 0 || drive > 1) return;
    // PCFDDコントローラの設定更新コードをここに追加
    // RPM設定が変わった場合に、MODE_SELECT_DOSVを切り替える
    switch (ctx->drive[drive].rpm_control) {
    case FDD_RPM_CONTROL_360:
        // 360RPMモード
        GPIOB->BSHR = (1 << 0);  // MODE_SELECT_DOSV = 1
        ctx->drive[drive].rpm_setting = FDD_RPM_360;
        ctx->drive[drive].rpm_measured = FDD_RPM_UNKNOWN;
        ctx->drive[drive].bps_measured = BPS_UNKNOWN;
        break;
    case FDD_RPM_CONTROL_300:
        // 300RPMモード
        GPIOB->BCR = (1 << 0);  // MODE_SELECT_DOSV = 0
        ctx->drive[drive].rpm_setting = FDD_RPM_300;
        ctx->drive[drive].rpm_measured = FDD_RPM_UNKNOWN;
        ctx->drive[drive].bps_measured = BPS_UNKNOWN;
        break;
    case FDD_RPM_CONTROL_9SCDRV:
        // 9SCDRV互換モード
        break;
    default:
        break;
    }
}

void pcfdd_try_eject(minyasx_context_t* ctx, int drive) {
    if (drive < 0 || drive > 1) return;
    if (ctx->drive[drive].eject_masked) {
        return;
    }
    pcfdd_force_eject(ctx, drive);
}

void pcfdd_force_eject(minyasx_context_t* ctx, int drive) {
    if (drive < 0 || drive > 1) return;
    // PCFDDコントローラの強制イジェクトコードをここに追加
    ctx->drive[drive].force_ejected = true;
    ctx->drive[drive].inserted = false;
    ctx->drive[drive].ready = false;
    ctx->drive[drive].rpm_measured = FDD_RPM_UNKNOWN;
    ctx->drive[drive].bps_measured = BPS_UNKNOWN;
}

/**
 * メディアの存在を調べ、挿入されていたらinsertedをtrueにする
 */
void pcfdd_detect_media(minyasx_context_t* ctx, int drive) {
    if (drive < 0 || drive > 1) return;
    drive_status_t* d = &ctx->drive[drive];
    if (!d->connected) return;
    if (!d->force_ejected && d->inserted) return;  // 既に挿入済み

    // TODO: 実際にFDメディアが入っているかを確認した上で設定する
    d->force_ejected = false;
    d->inserted = true;
    d->ready = true;
    d->rpm_measured = FDD_RPM_UNKNOWN;
    d->bps_measured = BPS_UNKNOWN;
}
