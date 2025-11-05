#include "led_control.h"

#include <stdio.h>
#include <string.h>

#include "ch32fun.h"
#include "minyasx.h"
#include "ui/ui_control.h"

// #define WSRBG //For WS2816C's.
#define WSGRB  // For SK6805-EC15
#define NR_LEDS 4

// LED配置:
// LED 0: FDD Aのアクセスランプ
// LED 1: FDD Aのイジェクトランプ
// LED 2: FDD Bのアクセスランプ
// LED 3: FDD Bのイジェクトランプ

// LED色定義 (GRB形式: SK6805-EC15用)
#define LED_OFF 0x000000
#define LED_GREEN 0xFF0000  // G=255, R=0, B=0
#define LED_BLUE 0x0000FF   // G=0, R=0, B=255
#define LED_RED 0x00FF00    // G=0, R=255, B=0
#define LED_WHITE 0xFFFFFF  // G=255, R=255, B=255

// 明るさ調整（1/8に減光）
static inline uint32_t scale_brightness(uint32_t color) {
    uint32_t g = (color >> 16) & 0xFF;
    uint32_t r = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    g >>= 3;
    r >>= 3;
    b >>= 3;
    return (g << 16) | (r << 8) | b;
}

// LED状態保持用（4個分）
static uint32_t led_colors[NR_LEDS];

// グローバルコンテキスト保存用
static minyasx_context_t* g_led_ctx = NULL;

// LEDテストモード状態
static led_test_mode_t led_test_mode = LED_TEST_MODE_NORMAL;

// 点滅制御用
#define BLINK_INTERVAL_MS 500
static uint32_t last_blink_toggle_ms = 0;
static bool blink_state = false;

// WS2812BのコールバックでLED色を返す
uint32_t WS2812BLEDCallback(int ledno) {
    if (ledno < 0 || ledno >= NR_LEDS) {
        return LED_OFF;
    }
    return led_colors[ledno];
}

void WS2812_SPI_init() {
    WS2812BDMAInit();

    // 初期化：全LED消灯
    for (int i = 0; i < NR_LEDS; i++) {
        led_colors[i] = LED_OFF;
    }

    // 即座に消灯を反映
    WS2812BDMAStart(NR_LEDS);

    // 送信完了を待機
    int timeout = 10000;
    while (WS2812BLEDInUse && timeout > 0) {
        timeout--;
    }
}

void WS2812_SPI_clear() {
    ui_log(UI_LOG_LEVEL_INFO, "LED clear start\n");

    // テストモードをNORMALにリセット
    led_test_mode = LED_TEST_MODE_NORMAL;

    // 全LED消灯
    for (int i = 0; i < NR_LEDS; i++) {
        led_colors[i] = LED_OFF;
    }

    // DMAの完了を待機（タイムアウト付き）
    int timeout = 10000;
    while (WS2812BLEDInUse && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        ui_log(UI_LOG_LEVEL_WARN, "LED clear: DMA busy timeout (pre)\n");
    }

    // 即座に反映
    WS2812BDMAStart(NR_LEDS);

    // 送信完了を待機
    timeout = 10000;
    while (WS2812BLEDInUse && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        ui_log(UI_LOG_LEVEL_WARN, "LED clear: DMA busy timeout (post)\n");
    }

    ui_log(UI_LOG_LEVEL_INFO, "LED clear done\n");
}

void WS2812_SPI_set_test_mode(led_test_mode_t mode) {
    led_test_mode = mode;

    // DMAの完了を待機（タイムアウト付き）
    int timeout = 10000;
    while (WS2812BLEDInUse && timeout > 0) {
        timeout--;
    }

    // モードに応じてLEDを設定
    switch (mode) {
    case LED_TEST_MODE_ALL_ON:
        // 全LEDを白(暗め)で点灯
        for (int i = 0; i < NR_LEDS; i++) {
            led_colors[i] = scale_brightness(LED_WHITE);
        }
        break;
    case LED_TEST_MODE_ALL_OFF:
        // 全LED消灯
        for (int i = 0; i < NR_LEDS; i++) {
            led_colors[i] = LED_OFF;
        }
        break;
    case LED_TEST_MODE_BLINK:
        // 全LEDを点滅させる(処理はpollで行うので一旦消灯)
        for (int i = 0; i < NR_LEDS; i++) {
            led_colors[i] = LED_OFF;
        }
        break;
    case LED_TEST_MODE_NORMAL:
    default:
        // 通常モードに戻る（次のpollで更新される）
        return;
    }

    // 即座に反映
    WS2812BDMAStart(NR_LEDS);

    // 送信完了を待機
    timeout = 10000;
    while (WS2812BLEDInUse && timeout > 0) {
        timeout--;
    }
}

led_test_mode_t WS2812_SPI_get_test_mode() {
    return led_test_mode;
}

void WS2812_SPI_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    static uint32_t last_systick_ms = 0;
    // コンテキストを保存
    g_led_ctx = ctx;

    if (systick_ms - last_systick_ms < 100) {
        return;
    }
    last_systick_ms = systick_ms;

    // テストモード判定
    if (led_test_mode == LED_TEST_MODE_BLINK) {
        // 点滅モード
        // 点滅状態の更新（500msごとにトグル）
        if (systick_ms - last_blink_toggle_ms >= BLINK_INTERVAL_MS) {
            blink_state = !blink_state;
            last_blink_toggle_ms = systick_ms;
        }
        // 全LEDを点滅させる
        uint32_t r = (systick_ms & 0xf00) >> 8;
        uint32_t g = (systick_ms & 0x0f0) >> 4;
        uint32_t b = (systick_ms & 0x00f);
        uint32_t base = (g << 20) | (r << 12) | (b << 4);  // GRB形式
        uint32_t color = blink_state ? scale_brightness(base) : LED_OFF;
        for (int i = 0; i < NR_LEDS; i++) {
            led_colors[i] = color;
        }
        // LED更新開始
        WS2812BDMAStart(NR_LEDS);
        return;
    }
    if (led_test_mode != LED_TEST_MODE_NORMAL) {
        return;
    }

    // 前回のDMA転送が完了するまで待機（非ブロッキング確認）
    if (WS2812BLEDInUse) {
        return;
    }

    // 点滅状態の更新（500msごとにトグル）
    if (systick_ms - last_blink_toggle_ms >= BLINK_INTERVAL_MS) {
        blink_state = !blink_state;
        last_blink_toggle_ms = systick_ms;
    }

    // ドライブセレクトの状態を取得（PA0=DriveA, PA1=DriveB）
    uint32_t gpio_state = GPIOA->INDR;
    bool drive_select_a = (gpio_state & (1 << 0)) != 0;
    bool drive_select_b = (gpio_state & (1 << 1)) != 0;

    // 各ドライブのLED状態を更新
    for (int drive = 0; drive < 2; drive++) {
        drive_status_t* drv = &ctx->drive[drive];
        int led_access = drive * 2;     // ドライブA=0, ドライブB=2
        int led_eject = drive * 2 + 1;  // ドライブA=1, ドライブB=3

        // アクセスランプの制御
        bool drive_selected = (drive == 0) ? drive_select_a : drive_select_b;

        if (drv->state == DRIVE_STATE_DISABLED) {
            // ドライブがDisableされている → 消灯
            led_colors[led_access] = LED_OFF;
        } else if ((drv->state != DRIVE_STATE_READY) && drv->led_blink) {
            // READY以外の状態でLED_BLINK有効 → 緑点滅
            // この時はドライブセレクトがあっても点灯しない
            led_colors[led_access] = blink_state ? scale_brightness(LED_GREEN) : LED_OFF;
        } else if (drv->state == DRIVE_STATE_READY) {
            // Ready状態
            if (drive_selected) {
                // アクセス中
                if (drv->rpm_setting == FDD_RPM_300) {
                    // 300RPMでアクセス中 → 青点灯
                    led_colors[led_access] = scale_brightness(LED_BLUE);
                } else {
                    // 360RPMでアクセス中 → 赤点灯
                    led_colors[led_access] = scale_brightness(LED_RED);
                }
            } else {
                // アクセスしていない → 緑点灯
                led_colors[led_access] = scale_brightness(LED_GREEN);
            }
        } else {
            // その他の状態 → 消灯
            led_colors[led_access] = LED_OFF;
        }

        // イジェクトランプの制御
        if (drv->state == DRIVE_STATE_DISABLED) {
            // ドライブがDisableされている → 消灯
            led_colors[led_eject] = LED_OFF;
        } else {
            if (!drv->eject_masked) {
                // イジェクト可能 → 緑色
                led_colors[led_eject] = scale_brightness(LED_GREEN);
            } else {
                // イジェクトマスク中 → 消灯
                led_colors[led_eject] = LED_OFF;
            }
        }
    }

    // LED更新開始
    WS2812BDMAStart(NR_LEDS);
}
