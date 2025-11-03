#include "pcfdd/pcfdd_control.h"

#include <stdbool.h>
#include <stdlib.h>

#include "ch32fun.h"
#include "greenpak/greenpak_control.h"
#include "ui/ui_control.h"

#define BPS_TIM_PRESCALER_SHIFT 3                         // 実際に取り込む頻度を下げる頻度 (IC1PSCの値。0=1/1, 1=1/2, 2=1/4, 3=1/8)
#define BPS_TIM_PRESCALER (1 << BPS_TIM_PRESCALER_SHIFT)  // 48MHz/(BPS_TIM_PRESCALER) = 4MHz(既定) → 0.25us resolution

#define SCALE (1u << BPS_TIM_PRESCALER_SHIFT)
#define S_LO(x) ((x) * SCALE)
#define S_HI(x) ((x) * SCALE + (SCALE - 1))  // 上側を「その刻みの最終tick」まで広げる

/* --- ドライブ別 DMA バッファ（CCR=CH1CVRは32bit。まずは32bit受け推奨） --- */
#define READ_DATA_CAP_N 1024
static volatile uint32_t cap_buf_ds0[READ_DATA_CAP_N];
static volatile uint32_t cap_buf_ds1[READ_DATA_CAP_N];
static volatile uint32_t* cap_buf_active = cap_buf_ds0; /* 切替用 */

/**
 * PC FDDのMODE SELECT信号を設定し、回転数変更を試みます
 * ただし、rpm_controlの設定が固定になっている場合は変更しません。
 */
void pcfdd_set_rpm_mode_select(drive_status_t* drive, fdd_rpm_mode_t rpm) {
    uint32_t flag = (1 << 0);  // MODE_SELECT_DOSV のビット位置
    bool inverted = drive->mode_select_inverted;

    switch (drive->rpm_control) {
    case FDD_RPM_CONTROL_300:
        rpm = FDD_RPM_300;
        break;
    case FDD_RPM_CONTROL_360:
        rpm = FDD_RPM_360;
        break;
    case FDD_RPM_CONTROL_NONE:
        // NONEの場合は、rpm引数は無視される
        // 回転数制御なしの場合は、ドライブのデフォルト動作に任せるので、MODE_SELECTはディアサートする
        GPIOB->BCR = flag;  // MODE_SELECT_DOSVをクリア
        drive->rpm_setting = FDD_RPM_UNKNOWN;
        return;
    case FDD_RPM_CONTROL_9SCDRV:
    case FDD_RPM_CONTROL_BPS:
        // これらのモードでは、rpm引数がそのまま使われる
        break;
    default:
        // 想定外の値の場合は、変更しない
        return;
    }
    drive->rpm_setting = rpm;
    // MODE_SELECT_DOSV の設定
    if (((rpm == FDD_RPM_300) && !inverted) || ((rpm == FDD_RPM_360) && inverted)) {
        // TODO:MODE_SELECT をGreenPAK のVirtual Input 経由で設定するように変更する
        //        GPIOB->BCR = flag;  // MODE_SELECT_DOSV = 300RPM mode
    } else {
        // TODO:MODE_SELECT をGreenPAK のVirtual Input 経由で設定するように変更する
        //        GPIOB->BSHR = flag;  // MODE_SELECT_DOSV = 360RPM mode
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
    const bool ds_a = (GPIOA->INDR & (1 << 0)) != 0;
    const bool ds_b = (GPIOA->INDR & (1 << 1)) != 0;
    if (ds_a && !ds_b) return DRIVE_A;
    if (!ds_a && ds_b) return DRIVE_B;
    // 想定外（両方0 or 両方1）は、前回を維持せず NONE とする
    return DRIVE_NONE;
}

void pcfdd_init(minyasx_context_t* ctx) {
    // PCFDDコントローラの初期化コードをここに追加
    for (int i = 0; i < 2; i++) {
        ctx->drive[i].state = DRIVE_STATE_POWER_OFF;
        ctx->drive[i].eject_masked = false;
        ctx->drive[i].led_blink = false;
        // rpm_controlはpreferences_init()で設定されるため、ここでは初期化しない
        // ctx->drive[i].rpm_control = FDD_RPM_CONTROL_9SCDRV;
        ctx->drive[i].rpm_setting = FDD_RPM_360;
        ctx->drive[i].rpm_measured = FDD_RPM_UNKNOWN;
        ctx->drive[i].bps_measured = BPS_UNKNOWN;
    }
    //
    // FDのINDEX信号(PA6)の立ち上がり/立ち下がりエッジを検出するために、Timer3 Channel1を使う
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

    // MODE_SELECT_DOSV の初期化
    pcfdd_set_rpm_mode_select(&ctx->drive[0], ctx->drive[0].rpm_setting);
    pcfdd_set_rpm_mode_select(&ctx->drive[1], ctx->drive[1].rpm_setting);
}

/*
  Timer3 IRQ: CC1IF -> エッジ検出（SysTick で幅計算）
              UIF   -> タイムアウト監視（現在の対象ドライブに対して）
 */
void TIM3_IRQHandler(void) __attribute__((interrupt));
void TIM3_IRQHandler(void) {
    drive_t drv = current_drive_from_gpio();
    uint32_t now_cycles = SysTick->CNT;

    // ---- UIF: タイムアウト監視（現在の対象ドライブに限定）----
    // スピーカーのPWMと兼用しているので、タイムアウトは起こらない場合があるので使えない
    // →ポーリングでタイムアウトを見ることにする
    if (TIM3->INTFR & TIM_UIF) {
        TIM3->INTFR &= ~TIM_UIF;
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
            s_last_edge_cycles = now_cycles;
            return;
        }

        // NONE（不明）なら今回のエッジは無視
        if (drv == DRIVE_NONE) return;

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

void step_dosv(int drive, bool direction_inward) {
    if (drive < 0 || drive > 1) {
        return;
    }
    // GreenPAK4の Vitrual Input に以下を接続している
    // 6 (bit1)  = DIRECTION (正論理)
    // 5 (bit2)  = STEP (正論理)
    // LOCK_ACKがアサートされている間だけ上記信号は有効になるので、
    // LOCKが取れている状態でこのメソッドを呼ぶこと
    uint8_t gp4_vin = greenpak_get_virtualinput(4 - 1);
    gp4_vin |= (1 << 2);  // bit2 = 1 (STEP = 1)
    if (direction_inward) {
        gp4_vin |= (1 << 1);  // bit1 = 1 (DIRECTION = 1, 内周方向)
    } else {
        gp4_vin &= ~(1 << 1);  // bit1 = 0 (DIRECTION = 0, 外周方向)
    }
    greenpak_set_virtualinput(4 - 1, gp4_vin);
    Delay_Ms(1);
    gp4_vin &= ~(1 << 2);  // bit2 = 0 (STEP = 0)
    greenpak_set_virtualinput(4 - 1, gp4_vin);
    Delay_Ms(3);
}

void drive_select(int drive, bool active) {
    if (drive < 0 || drive > 1) {
        return;
    }
    // GreenPAK1の Virtual Input 6/7 (bit1/0) に DS0/DS1 を接続している
    uint8_t gp1_vin = greenpak_get_virtualinput(1 - 1);
    if (active) {
        gp1_vin |= (1 << (1 - drive));  // bit1/0 = 1 (DS0/DS1 active)
    } else {
        gp1_vin &= ~(1 << (1 - drive));  // bit1/0 = 0 (DS0/DS1 inactive)
    }
    greenpak_set_virtualinput(1 - 1, gp1_vin);
}

void pcfdd_drive_select(int drive, bool active) {
    drive_select(drive, active);
}

bool seek_to_track0(int drive) {
    ui_logf(UI_LOG_LEVEL_DEBUG, "Seek Track0 (D:%d)\n", drive);
    if (drive < 0 || drive > 1) {
        return false;
    }

    int seek_count = 0;
    // シーク Part1
    drive_select(drive, true);
    for (int i = 0; i < 1; i++) {
        //        ui_logf(UI_LOG_LEVEL_DEBUG, " Step:%d)\n", i);
        step_dosv(drive, true);  // 内周方向にステップ
    }
    // ここで 100msec待つ(Track00以外でシークを一度確定しないとDISK_CHANGEがクリアされないドライブがある?)
    Delay_Ms(100);
    // シーク Part2
    seek_count = 0;
    const int SEEK_COUNT_MAX = 100;
    while (seek_count < SEEK_COUNT_MAX) {
        // TRACK0を見て、トラック0に到達したら抜ける
        if ((GPIOB->INDR & (1 << 11)) == 0) {
            // TRACK0_DOSV = 0 (Low) になった
            break;
        }
        step_dosv(drive, false);  // 外周方向にステップ
        seek_count++;
    }
    Delay_Ms(1);
    drive_select(drive, false);

    ui_logf(UI_LOG_LEVEL_DEBUG, "  Seeked %d steps\n", seek_count);
    // 戻り値: SEEK_COUNT_MAX ステップ以内にトラック0に到達したらtrue
    return (seek_count < SEEK_COUNT_MAX);
}

uint32_t last_index_ms = 0;
bool last_index_state = false;

/**
 * FDDバスのロックを取得します。
 * LOCK_REQをアサートすると、GP1は DRIVE_SELECT_A/Bが両方ともアクティブじゃないタイミングで
 * LOCK_ACK (PB7)をアサートします。
 * LOCK_ACKがアサートされている間は、READY以外の信号がX68000側に届かなくなります。
 * そうすると、X68000側にはINDEXが届かなくなるので、X68000のFDCはINDEXを待ち続けるため、
 * その隙に　PCFDD側のDrive Selectをアクティブにしても問題なくなります。
 */
static bool get_fdd_lock() {
    GPIOB->BSHR = GPIO_Pin_6;  // LOCK_REQ active (High=アクセス禁止要求)
    uint32_t systick_start = SysTick->CNTL;
    while ((GPIOB->INDR & GPIO_Pin_7) == 0) {
        // LOCK_ACKがアサートされるまで待つ
        // (100msec以上待っても来ない場合は、一旦リターンし次回の呼び出しで再度試みる)
        if ((SysTick->CNTL - systick_start) > (100 * 1000 * 48)) {
            // 100msec以上待ってもLOCK_ACKが来らない場合は失敗
            GPIOB->BCR = GPIO_Pin_6;  // LOCK_REQ inactive
            return false;             // LOCK取得失敗
        }
    }
    // LOCK_ACKがアサートされた(LOCK取得成功)
    return true;
}

static void release_fdd_lock() {
    GPIOB->BCR = GPIO_Pin_6;  // LOCK_REQ inactive
}

static void process_initializing(minyasx_context_t* ctx, int drive) {
    if (ctx->drive[drive].state != DRIVE_STATE_INITIALIZING) return;
    // 1. LOCK_REQをアサートして、LOCK_ACKがアサートされるまで待つ
    if (!get_fdd_lock()) {
        // LOCK取得失敗 (一旦リターンし次回の呼び出しで再度試みる)
        return;
    }

    // シークしてトラック0に戻す
    if (seek_to_track0(drive)) {
        ctx->drive[drive].state = DRIVE_STATE_MEDIA_DETECTING;
        ctx->drive[drive].rpm_measured = FDD_RPM_UNKNOWN;
        ctx->drive[drive].bps_measured = BPS_UNKNOWN;
    } else {
        // トラック0に戻れなかった =  ドライブが存在しない
        ctx->drive[drive].state = DRIVE_STATE_NOT_CONNECTED;
        ctx->drive[drive].rpm_measured = FDD_RPM_UNKNOWN;
        ctx->drive[drive].bps_measured = BPS_UNKNOWN;
    }

    // ロックを解除する
    release_fdd_lock();
}

static void process_media_detecting(minyasx_context_t* ctx, int drive, uint32_t systick_ms) {
    if (ctx->drive[drive].state != DRIVE_STATE_MEDIA_DETECTING) return;

    drive_status_t* d = &ctx->drive[drive];

    // 0. READY_MCU_n/DISK_IN_x_nはディアサートし、もFDC側にメディア無しを通知する
    GPIOB->BSHR = (drive == 0) ? GPIO_Pin_12 : GPIO_Pin_13;  // READY_MCU_A_n / READY_MCU_B_n (High=準備完了でない)
    uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
    gp3_vin |= (1 << (5 - drive));  // bit4/5を0にして、DISK_IN_x_nをDisableにする
    greenpak_set_virtualinput(3 - 1, gp3_vin);

    // 1. LOCK_REQをアサートして、LOCK_ACKがアサートされるまで待つ
    ui_logf(UI_LOG_LEVEL_DEBUG, "Media Detecting (%d)\n", drive);
    if (!get_fdd_lock()) {
        // LOCK取得失敗 (一旦リターンし次回の呼び出しで再度試みる)
        ui_logf(UI_LOG_LEVEL_DEBUG, " Lock failed\n");
        return;
    }
    ui_logf(UI_LOG_LEVEL_DEBUG, " Lock acquired\n");

    // TODO: ここでDRIVE_SELECT_A/Bが両方ともアクティブでないことを確認する

    // 2. TRACK00にシークする
    // これをするとメディアが入っている時はPC FDDの DISK_CHANGEもクリアされる
    // (メディアが入っていない時はDISK_CHANGEはクリアされない=常にLow)
    seek_to_track0(drive);

    // 3. PC FDD側のDrive Selectをアクティブにし、DISK_CHANGEがアクティブかを確認する
    drive_select(drive, true);
    bool disk_change = (GPIOB->INDR & (1 << 4)) == 0;  // DISK_CHANGE_DOSV = 0 (Low) active
    if (disk_change) {
        // シークしたのにDISK_CHANGEがアクティブなまま
        // →メディアが入っていないと判断する
        drive_select(drive, false);  // Drive Selectを非アクティブにする
        // MEDIA_WAITING状態に遷移
        // タイムスタンプが0の場合（初回）のみ更新
        if (d->media_waiting_start_ms == 0) {
            d->media_waiting_start_ms = systick_ms;
        }
        d->state = DRIVE_STATE_MEDIA_WAITING;
        d->rpm_measured = FDD_RPM_UNKNOWN;
        d->bps_measured = BPS_UNKNOWN;
        uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
        gp3_vin |= (1 << (5 - drive));  // bit4/5を1にして、DISK_IN_x_nをDisableにする
        greenpak_set_virtualinput(3 - 1, gp3_vin);
        release_fdd_lock();
        ui_logf(UI_LOG_LEVEL_DEBUG, " No Media (Disk Change)\n");
        return;
    }

    // 4. メディアがありそうなのでMOTOR_ONもアクティブにし、INDEX計測できるようにする
    if ((GPIOA->INDR & (1 << 12)) == 0) {
        // MOTOR_ON_GP (PA12) がアクティブじゃない場合は、アクティブにする
        ui_logf(UI_LOG_LEVEL_DEBUG, " Motor On\n");
        uint8_t gp4_in = greenpak_get_virtualinput(4 - 1);
        gp4_in |= (1 << 0);  // bit0 = 1 (MOTOR_ON = 1)
        greenpak_set_virtualinput(4 - 1, gp4_in);
        // モーターの回転が安定するまで 300msec待つ
        Delay_Ms(300);
    }

    // 5. 1000msecの間に INDEXパルスが来るかを監視する
    uint32_t systick_start = SysTick->CNTL;
    bool index_low_seen = false;
    bool index_high_seen = false;
    while ((SysTick->CNTL - systick_start) < (1000 * 1000 * 48)) {
        uint32_t gpioa = GPIOA->INDR;
        if ((gpioa & (1 << 6)) == 0) {
            // INDEX_DOSV (PA6) = 0 (Low) になった
            index_low_seen = true;
        }
        if ((gpioa & (1 << 6)) != 0) {
            // INDEX_DOSV (PA6) = 1 (High) になった
            index_high_seen = true;
        }
        if (index_low_seen && index_high_seen) {
            break;
        }
    }

    // 5. INDEXパルスの有無で、メディアの有無を判断する
    if (!(index_low_seen && index_high_seen)) {
        // INDEXパルスが来なかった
        // →メディア無しと判断する
        ui_logf(UI_LOG_LEVEL_INFO, " No Index Pulse\n");
        // MEDIA_WAITING状態に遷移
        // タイムスタンプが0の場合（初回）のみ更新
        if (d->media_waiting_start_ms == 0) {
            d->media_waiting_start_ms = systick_ms;
        }
        d->state = DRIVE_STATE_MEDIA_WAITING;
        d->rpm_measured = FDD_RPM_UNKNOWN;
        d->bps_measured = BPS_UNKNOWN;
        uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
        gp3_vin |= (1 << (5 - drive));  // bit4/5を1にして、DISK_IN_x_nをDisableにする
        greenpak_set_virtualinput(3 - 1, gp3_vin);
    } else {
        // INDEXパルスが来た
        // →メディア有りと判断する
        d->state = DRIVE_STATE_READY;
        d->media_waiting_start_ms = 0;  // タイムスタンプをクリア
        d->rpm_measured = FDD_RPM_UNKNOWN;
        d->bps_measured = BPS_UNKNOWN;
        uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
        gp3_vin &= ~(1 << (5 - drive));  // bit4/5を0にして、DISK_IN_x_nをEnableにする
        greenpak_set_virtualinput(3 - 1, gp3_vin);
    }

    // 6. Drive Selectを非アクティブにし、MOTOR_ONも非アクティブにする
    drive_select(drive, false);

    uint8_t gp4_in = greenpak_get_virtualinput(4 - 1);
    gp4_in &= ~(1 << 0);  // bit0 = 0 (MOTOR_ON_DOSV = 0)
    greenpak_set_virtualinput(4 - 1, gp4_in);

    // 6. ロックを解除する
    release_fdd_lock();

    return;
}

static void process_media_waiting(minyasx_context_t* ctx, int drive, uint32_t systick_ms) {
    if (ctx->drive[drive].state != DRIVE_STATE_MEDIA_WAITING) return;

    // メディア自動検出設定に応じた処理
    media_auto_detect_t mode = ctx->preferences.media_auto_detect;

    if (mode == MEDIA_AUTO_DETECT_DISABLED) {
        // 自動検出しない：即座にEJECTED状態に遷移
        ctx->drive[drive].state = DRIVE_STATE_EJECTED;
        ctx->drive[drive].media_waiting_start_ms = 0;
        ui_logf(UI_LOG_LEVEL_INFO, "Drive %d: MEDIA_WAITING -> EJECTED (auto-detect disabled)\n", drive);
        return;
    } else if (mode == MEDIA_AUTO_DETECT_60SEC) {
        // 60秒間トライ：60秒経過したらEJECTED状態に遷移
        if (ctx->drive[drive].media_waiting_start_ms == 0) {
            // タイムスタンプが0の場合は、ここで更新する
            ctx->drive[drive].media_waiting_start_ms = systick_ms;
        }
        uint32_t elapsed = systick_ms - ctx->drive[drive].media_waiting_start_ms;
        if (elapsed >= 60000) {
            ctx->drive[drive].state = DRIVE_STATE_EJECTED;
            ctx->drive[drive].media_waiting_start_ms = 0;
            ui_logf(UI_LOG_LEVEL_INFO, "Drive %d: MEDIA_WAITING timeout -> EJECTED\n", drive);
            return;
        }
    }
    // MEDIA_AUTO_DETECT_UNLIMITED の場合はタイムアウトしない（何もしない）

    // メディア待ち状態の処理
    // 1. READY_MCUをInactiveにする
    GPIOB->BSHR = (drive == 0) ? GPIO_Pin_12 : GPIO_Pin_13;  // READY_MCU_A_n / READY_MCU_B_n (High=準備完了でない)
    // 2. GP2,GP3の DISK_IN_x_n をDisableにする
    uint8_t gp2_vin = greenpak_get_virtualinput(2 - 1);
    gp2_vin |= (1 << (5 - drive));  // bit4/5を1にして、DISK_IN_x_nをDisableにする
    greenpak_set_virtualinput(2 - 1, gp2_vin);
    uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
    gp3_vin |= (1 << (5 - drive));  // bit4/5を1にして、DISK_IN_x_nをDisableにする
    greenpak_set_virtualinput(3 - 1, gp3_vin);
}

static void process_ejected(minyasx_context_t* ctx, int drive) {
    if (ctx->drive[drive].state != DRIVE_STATE_EJECTED) return;

    // EJECTED状態の処理 (メディア検出をストップした状態)
    // 1. READY_MCUをInactiveにする
    GPIOB->BSHR = (drive == 0) ? GPIO_Pin_12 : GPIO_Pin_13;  // READY_MCU_A_n / READY_MCU_B_n (High=準備完了でない)
    // 2. GP2,GP3の DISK_IN_x_n をDisableにする
    uint8_t gp2_vin = greenpak_get_virtualinput(2 - 1);
    gp2_vin |= (1 << (5 - drive));  // bit4/5を1にして、DISK_IN_x_nをDisableにする
    greenpak_set_virtualinput(2 - 1, gp2_vin);
    uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
    gp3_vin |= (1 << (5 - drive));  // bit4/5を1にして、DISK_IN_x_nをDisableにする
    greenpak_set_virtualinput(3 - 1, gp3_vin);
}

static void process_ready(minyasx_context_t* ctx, int drive) {
    if (ctx->drive[drive].state != DRIVE_STATE_READY) return;

    // READY状態の処理

    // 1. READY_MCUをActiveにする
    // MOTOR ON信号(PA12)がアクティブならREADY信号をアクティブにする
    // GreenPAKは各ドライブにDriveSelect信号がアサートされると、
    // このREADY信号の値を返却します
    if (!(GPIOA->INDR & GPIO_Pin_12)) {
        // MOTOR_ON アクティブ
        GPIOB->BCR = (drive == 0) ? GPIO_Pin_12 : GPIO_Pin_13;  // READY_MCU_A_n / READY_MCU_B_n (Low=準備完了)
    } else {
        GPIOB->BSHR = (drive == 0) ? GPIO_Pin_12 : GPIO_Pin_13;  // READY_MCU_A_n / READY_MCU_B_n (High=準備完了でない)
    }

    // 2. GP2,GP3の DISK_IN_x_n をEnableにする
    uint8_t gp2_vin = greenpak_get_virtualinput(2 - 1);
    gp2_vin &= ~(1 << (5 - drive));  // bit4/5を0にして、DISK_IN_x_nをEnableにする
    greenpak_set_virtualinput(2 - 1, gp2_vin);

    uint8_t gp3_vin = greenpak_get_virtualinput(3 - 1);
    gp3_vin &= ~(1 << (5 - drive));  // bit4/5を0にして、DISK_IN_x_nをEnableにする
    greenpak_set_virtualinput(3 - 1, gp3_vin);
}

void pcfdd_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    // PCFDDコントローラの定期処理コード
    for (int drive = 0; drive < 2; drive++) {
        switch (ctx->drive[drive].state) {
        case DRIVE_STATE_POWER_OFF:
            break;
        case DRIVE_STATE_INITIALIZING:
            process_initializing(ctx, drive);
            break;
        case DRIVE_STATE_NOT_CONNECTED:
            break;
        case DRIVE_STATE_DISABLED:
            break;
        case DRIVE_STATE_MEDIA_DETECTING:
            process_media_detecting(ctx, drive, systick_ms);
            break;
        case DRIVE_STATE_MEDIA_WAITING:
            process_media_waiting(ctx, drive, systick_ms);
            break;
        case DRIVE_STATE_EJECTED:
            process_ejected(ctx, drive);
            break;
        case DRIVE_STATE_READY:
            process_ready(ctx, drive);
            break;
        default:
            break;
        }
    }

    // IN_USE信号の生成
    for (int drive = 0; drive < 2; drive++) {
        bool drive_select = (GPIOA->INDR & (1 << (0 + drive))) != 0;  // Drive Select A/B active?
        if ((ctx->drive[drive].in_use_mode == FDD_IN_USE_LED) && drive_select) {
            // Drive Selectがアクティブな間だけIN_USEもアクティブにする
            // Drive Selectがアクティブ
            GPIOB->BSHR = GPIO_Pin_1;
        } else {
            // それ以外はIN_USEを非アクティブにする
            GPIOB->BCR = GPIO_Pin_1;
        }
    }

    //
    // メディアの抜き差し検出
    //

    // DSがアサートされているかどうかを見て、その際にDISK_CHANGE_DOSV (PB8) のアサートを監視し、
    // もしDISCK_CHANGEを検出したらメディア検出状態に遷移する
    bool disk_change_det[2] = {true, true};
    for (int drive = 0; drive < 2; drive++) {
        uint32_t systick_start = SysTick->CNTL;
        while ((SysTick->CNTL - systick_start) < (10 * 1000 * 48)) {      // 10msecだけ監視
            bool drive_select = (GPIOA->INDR & (1 << (0 + drive))) != 0;  // Drive Select A/B active?
            bool disk_change = (GPIOB->INDR & (1 << 8)) == 0;             // DISK_CHANGE_DOSV = 0 (Low) active
            if (!drive_select || !disk_change) {
                disk_change_det[drive] = false;
                break;
            }
        }
    }

    // DSがアサートされていない場合はDISK_CHANGEの検知ができないので、
    // 定期的にロックを取得して、DSをアサートしてDISK_CHANGEを監視する
    static uint32_t last_media_check_time = 0;
    if (systick_ms - last_media_check_time > 3000) {
        last_media_check_time = systick_ms;
        // 3秒に一度、LOCKを取得してDISK_CHANGEを監視する
        if (get_fdd_lock()) {
            // LOCK取得成功
            for (int drive = 0; drive < 2; drive++) {
                //  以下の状態のドライブはスキップ
                switch (ctx->drive[drive].state) {
                case DRIVE_STATE_POWER_OFF:
                case DRIVE_STATE_NOT_CONNECTED:
                case DRIVE_STATE_DISABLED:
                case DRIVE_STATE_INITIALIZING:
                case DRIVE_STATE_EJECTED:
                    continue;
                default:
                    break;
                }
                drive_select(drive, true);
                Delay_Ms(1);                                       // 少し待つ
                bool disk_change = (GPIOB->INDR & (1 << 4)) == 0;  // DISK_CHANGE_DOSV = 0 (Low) active
                if (disk_change) {
                    Delay_Ms(10);                                 // 念のためもう一度確認する (10msec)
                    disk_change = (GPIOB->INDR & (1 << 4)) == 0;  // DISK_CHANGE_DOSV = 0 (Low) active
                    if (disk_change) {
                        // 2回ともDISK_CHANGEがアクティブだった
                        ui_logf(UI_LOG_LEVEL_TRACE, "DCHG detected on D%d during polling\n", drive);
                        disk_change_det[drive] = true;
                    }
                }
                drive_select(drive, false);
            }
            release_fdd_lock();
        }
    }

    // DISK_CHANGEが検出された場合の処理
    for (int drive = 0; drive < 2; drive++) {
        drive_status_t* drv = &ctx->drive[drive];
        if (disk_change_det[drive]) {
            ui_logf(UI_LOG_LEVEL_TRACE, "DCHG detected on D%d\n", drive);
            if (drv->state == DRIVE_STATE_READY) {
                // READY状態でDISK_CHANGEがアサートされたらメディア検出状態に遷移する
                // アクセス中なのでちょっと怖いが……
                drv->state = DRIVE_STATE_MEDIA_DETECTING;
            }
            if (drv->state == DRIVE_STATE_MEDIA_WAITING) {
                // MEDIA_WAITINGでDISK_CHANGEがアサートされたらメディア検出状態に遷移する
                // ただし、タイムスタンプは保持する（MEDIA_WAITING開始時刻を引き継ぐ）
                uint32_t saved_timestamp = drv->media_waiting_start_ms;
                drv->state = DRIVE_STATE_MEDIA_DETECTING;
                drv->media_waiting_start_ms = saved_timestamp;
            }
            if (drv->state == DRIVE_STATE_EJECTED) {
                // EJECTEDでDISK_CHANGEがアサートされたらメディア検出状態に遷移する
                drv->state = DRIVE_STATE_MEDIA_DETECTING;
            }
        }
    }

    //
    // RPMとBPSの計測を1秒毎に行う
    //
    static uint64_t last_tick = 0;
    if (systick_ms - last_tick < 1000) {
        return;
    }
    last_tick = systick_ms;

    // RPMのタイムアウト監視
    {
        uint32_t now_cycles = SysTick->CNT;
        drive_t drv = current_drive_from_gpio();
        if (drv == DRIVE_NONE) {
            // NONEなら(ドライブがどちらもアクティブでない場合)一旦リセット
            s_current_drive = DRIVE_NONE;
            s_have_prev_edge = 0;
            s_last_edge_cycles = 0;
        } else if (s_current_drive == DRIVE_NONE) {
            // NONE→A/Bに変わった場合は、基準確立からやり直し
            s_current_drive = drv;
            s_have_prev_edge = 0;
            s_last_edge_cycles = now_cycles;

        } else {
            // ここではタイムアウトのみ検出
            // INDEXパルスが来ていれば、後述の「CC1IF」で処理される
            uint32_t delta_us = (now_cycles - s_last_edge_cycles) / 48u;
            if (delta_us > TIMEOUT_US) {
                index_width[drv] = 0;  // 500ms 以上エッジ無し → タイムアウト
                                       // 基準は保持（次のエッジで復帰）
            }
        }
    }

    // RPMの計測結果を反映
    for (int drive = 0; drive < 2; drive++) {
        if (index_width[drive] == 0) {
            // タイムアウト中
            ctx->drive[drive].rpm_measured = FDD_RPM_UNKNOWN;
            continue;
        }
        int margin = (200 - 166) / 2;
        if (166 - margin < index_width[drive] && index_width[drive] < 166 + margin) {
            ctx->drive[drive].rpm_measured = FDD_RPM_360;
        } else if (200 - margin < index_width[drive] && index_width[drive] < 200 + margin) {
            ctx->drive[drive].rpm_measured = FDD_RPM_300;
        } else {
            // それ以外は無視
            (void)0;
        }
    }

    // BPSの計測結果を反映
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
    drive_status_t* d = &ctx->drive[drive];
    switch (d->rpm_control) {
    case FDD_RPM_CONTROL_360:
        // 360RPMモード
        if (d->mode_select_inverted) {
            // MODE_SELECT_DOSV = 0
            // TODO:MODE_SELECT をGreenPAK のVirtual Input 経由で設定するように変更する
            GPIOB->BCR = (1 << 0);  // MODE_SELECT_DOSV = 0
        } else {
            // TODO:MODE_SELECT をGreenPAK のVirtual Input 経由で設定するように変更する
            GPIOB->BSHR = (1 << 0);  // MODE_SELECT_DOSV = 1
        }
        d->rpm_setting = FDD_RPM_360;
        d->rpm_measured = FDD_RPM_UNKNOWN;
        d->bps_measured = BPS_UNKNOWN;
        break;
    case FDD_RPM_CONTROL_300:
        // 300RPMモード
        if (d->mode_select_inverted) {
            // MODE_SELECT_DOSV = 1
            // TODO:MODE_SELECT をGreenPAK のVirtual Input 経由で設定するように変更する
            GPIOB->BSHR = (1 << 0);  // MODE_SELECT_DOSV = 1
        } else {
            // TODO:MODE_SELECT をGreenPAK のVirtual Input 経由で設定するように変更する
            GPIOB->BCR = (1 << 0);  // MODE_SELECT_DOSV = 0
        }
        d->rpm_setting = FDD_RPM_300;
        d->rpm_measured = FDD_RPM_UNKNOWN;
        d->bps_measured = BPS_UNKNOWN;
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
    drive_status_t* d = &ctx->drive[drive];
    if (d->state != DRIVE_STATE_READY) {
        return;
    }
    // ui_logf(UI_LOG_LEVEL_INFO, "Drive %d: Ejecting media\n", drive);
    d->state = DRIVE_STATE_MEDIA_WAITING;
    d->rpm_measured = FDD_RPM_UNKNOWN;
    d->bps_measured = BPS_UNKNOWN;
    d->media_waiting_start_ms = 0;  // タイムスタンプをクリア
}

/**
 * メディアの存在を調べ、挿入されていたらinsertedをtrueにする
 */
void pcfdd_detect_media(minyasx_context_t* ctx, int drive) {
    if (drive < 0 || drive > 1) return;
    drive_status_t* d = &ctx->drive[drive];
    if ((d->state != DRIVE_STATE_MEDIA_WAITING) &&  //
        (d->state != DRIVE_STATE_EJECTED) &&        //
        (d->state != DRIVE_STATE_READY)) {
        return;
    }
    d->state = DRIVE_STATE_MEDIA_DETECTING;
    d->rpm_measured = FDD_RPM_UNKNOWN;
    d->bps_measured = BPS_UNKNOWN;
}

char* pcfdd_state_to_string(drive_state_t state) {
    switch (state) {
    case DRIVE_STATE_POWER_OFF:
        return "------";
    case DRIVE_STATE_NOT_CONNECTED:
        return "******";
    case DRIVE_STATE_DISABLED:
        return "Disabl";
    case DRIVE_STATE_INITIALIZING:
        return "Init..";
    case DRIVE_STATE_MEDIA_DETECTING:
        return ">>>>>>";
    case DRIVE_STATE_MEDIA_WAITING:
        return "Waitng";
    case DRIVE_STATE_EJECTED:
        return "Ejectd";
    case DRIVE_STATE_READY:
        return "Ready ";
    default:
        return "Unknwn";
    }
}
