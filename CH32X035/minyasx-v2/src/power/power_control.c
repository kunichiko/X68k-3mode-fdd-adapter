// Minyas Xの電源制御
//
// 起動後に以下の初期かを行います。
//
// ● 0.+12V電源ライン、+5V電源ラインのDisable
// 間違って+12Vが入らないように、起動直後に+12VラインをDisableします。
// +5Vラインも同様にDisableします。
// マイコンはVBUSに直結したレギュレータで動作するのでもんだいありません。
//
// ● 1.外部+12V電源の検出
// 外部+12V電源が接続されているかどうかを調べます。
// 外部+12V電源が接続されていれば、+12V_EXT_DETがLowになります。
// この場合は、+12V_EXT ラインをEnableします。
//
// ● 2.USB-PDのネゴシエーション
// 外部電源がない場合は、PD電源から +12V, 1.6A以上の供給が可能かどうかを調べます。
// 可能ならVBUSを+12Vに設定します。
//
// ● 3.INA3221で+12Vラインの電圧をチェック
// +12Vに切り替わったことを確認できたら、+12VラインをEnableします。
// もし+12Vが来ていなければ、PDのネゴシエーションを+5Vに切り替えます。
//
// ● 4.INA3221で+5Vラインの電圧をチェック
// +5Vに切り替わったことを確認できたら、+5VラインをEnableします。
//
// ● 5.PDのネゴシエーション失敗時
// PDのネゴシエーションに失敗した場合は、+5VラインをEnableします。
// これは、USBバスパワーで動作させるためです。

#include "power/power_control.h"

#include "greenpak/greenpak_control.h"
#include "ina3221/ina3221_control.h"
#include "ui/ui_control.h"
#include "usbpd/usbpd_sink.h"

static bool fdd_power_enabled = false;
void enable_fdd_power(minyasx_context_t* ctx, bool enable);

// 電源状態管理
static power_state_t power_state = {
    .pvd_enabled = false,
    .low_voltage_detected = false,
    .x68k_power_on = false,
    .fdd_power_on = false,
};

void power_control_init(minyasx_context_t* ctx) {
    ctx->usbpd.connected = false;

    // ● 0.+12V電源ライン、+5V電源ラインのDisable
    GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
    GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
    GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)

    //
    Delay_Ms(500);

    if (!PD_connect()) {
        ui_print(UI_PAGE_LOG, "USBPD_Init error");
        ui_write(UI_PAGE_LOG, '\n');
        Delay_Ms(500);
        return;
    }

    ui_print(UI_PAGE_LOG, "USBPD_Init OK");
    ui_write(UI_PAGE_LOG, '\n');
    ctx->usbpd.connected = true;
    ctx->usbpd.pdonum = PD_getPDONum();

    for (int i = 1; i <= PD_getPDONum(); i++) {
        if (i <= PD_getFixedNum()) {
            ui_printf(UI_PAGE_LOG, " (%d)%6dmV %5dmA\n", i, PD_getPDOVoltage(i), PD_getPDOMaxCurrent(i));
            // USB-PD情報をコンテキストに保存
            if (i < 8) {
                ctx->usbpd.pod[i - 1].voltage_mv = PD_getPDOVoltage(i);
                ctx->usbpd.pod[i - 1].current_ma = PD_getPDOMaxCurrent(i);
            }
        } else {
            ui_printf(UI_PAGE_LOG, " [%d]%6dmV-%5dmV\n", i, PD_getPDOMinVoltage(i), PD_getPDOMaxVoltage(i));
        }
    }
}

/**
 * @brief USB-PDで+12Vを要求して有効化する
 *     +12Vが有効化できたらtrueを返す
 *     +12Vが有効化できなかったらfalseを返す
 */
bool activate_pd_12v(minyasx_context_t* ctx) {
    if (!ctx->usbpd.connected) {
        return false;
    }
    int pd_12v_pdo = -1;
    for (int i = 1; i <= ctx->usbpd.pdonum; i++) {
        ui_logf(UI_LOG_LEVEL_INFO, " (%d)%6dmV %5dmA\n", i, ctx->usbpd.pod[i - 1].voltage_mv, ctx->usbpd.pod[i - 1].current_ma);
        if (ctx->usbpd.pod[i - 1].voltage_mv == 12000 && ctx->usbpd.pod[i - 1].current_ma >= 1600) {
            // 12V, 1.6A以上のPDOを発見
            pd_12v_pdo = i;
        }
    }

    if (pd_12v_pdo >= 0) {
        ui_print(UI_PAGE_LOG, " -> request 12V\n");
        if (PD_setPDO(pd_12v_pdo, 12000)) {
            ui_print(UI_PAGE_LOG, " -> 12V OK\n");
            // +12Vに切り替わったことを確認できたら、+12VラインをEnableします。
            GPIOA->BSXR = (1 << (19 - 16));  // Enable (+12V_EN=High)
            return true;
        } else {
            ui_print(UI_PAGE_LOG, " -> 12V NG\n");
            pd_12v_pdo = -1;
        }
    } else {
        ui_print(UI_PAGE_LOG, " -> no 12V PDO\n");
    }
    return false;
}

/**
 * @brief FDDの電源をON/OFFする
 *
 * @param enable trueでON, falseでOFF
 *
 * @note
 * FDDの電源は、外部+12V電源ソースがある場合、PDから+12Vが供給できている場合、どちらも供給されていない場合によって
 * 以下のように制御します。
 * - 外部+12V電源ソースがある場合
 *   - FDDの電源ON: +12V_EXT_EN=High, +12V_EN=Low, +5V_EN=Low
 *   - FDDの電源OFF: +12V_EXT_EN=Low, +12V_EN=Low, +5V_EN=Low
 * - PDから+12Vが供給できている場合
 *   - FDDの電源ON: +12V_EXT_EN=Low, +12V_EN=High, +5V_EN=Low
 *   - FDDの電源OFF: +12V_EXT_EN=Low, +12V_EN=Low, +5V_EN=Low
 * - どちらも供給されていない場合 (VBUSが+5V)
 *   - FDDの電源ON: +12V_EXT_EN=Low, +12V_EN=Low, +5V_EN=High
 *   - FDDの電源OFF: +12V_EXT_EN=Low, +12V_EN=Low, +5V_EN=Low
 *
 *  GPIOのマッピングは以下の通り
 *   PA17: +12V_EXT_DET (Low=外部+12V電源接続, Pull-Up)
 *   PA18: +5V_EN (Low=Disable, High=Enable)
 *   PA19: +12V_EN (Low=Disable, High=Enable)
 *   PA20: +12V_EXT_EN (Low=Disable, High=Enable))
 */
void enable_fdd_power(minyasx_context_t* ctx, bool enable) {
    if (enable) {
        // ● 1.外部+12V電源の検出
        // 外部+12V電源が接続されていれば、+12V_EXT_DETがLowになります。
        // この場合は、+12V_EXT ラインをEnableします。
        if ((GPIOA->INDR & (1 << 17)) == 0) {
            // 外部+12V電源が接続されている
            GPIOA->BSXR = (1 << (20 - 16));  // Enable (+12V_EXT_EN=High)
            ui_log(UI_LOG_LEVEL_INFO, "+12V_EXT_DET active\n");
        } else {
            // 外部+12V電源が接続されていない
            // ● 2.USB-PDのネゴシエーション
            // 外部電源がない場合は、PD電源から +12V, 1.6A以上の供給が可能かどうかを調べます。
            bool pd_12v_enabled = activate_pd_12v(ctx);
            if (!pd_12v_enabled) {
                // +12Vが有効化できなかった場合は、VBUSが+5Vなので、+5VラインをEnableします。
                // 念の為、INA3221で VBUS(ch1)の電圧をチェックします。
                uint16_t ch1_current, ch1_voltage, ch2_current, ch2_voltage, ch3_current, ch3_voltage;
                ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);
                if (ch1_voltage >= 4750 && ch1_voltage <= 5500) {
                    ui_log(UI_LOG_LEVEL_INFO, "VBUS 5V OK\n");
                    GPIOA->BSXR = (1 << (18 - 16));  // Enable (+5V_EN=High)
                } else {
                    ui_logf(UI_LOG_LEVEL_ERROR, "VBUS 5V NG %dmV\n", ch1_voltage);
                }
            }
        }
        fdd_power_enabled = true;
        power_state.fdd_power_on = true;
        // ドライブの初期化を始める
        for (int i = 0; i < 2; i++) {
            if (ctx->drive[i].state == DRIVE_STATE_DISABLED) {
                continue;
            }
            ctx->drive[i].state = DRIVE_STATE_INITIALIZING;
        }
    } else {
        // FDDの電源OFF
        GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)
        GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
        GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
        fdd_power_enabled = false;
        power_state.fdd_power_on = false;
    }
}

bool fdd_power_is_enabled(void) {
    return fdd_power_enabled;
}

static bool is_x68k_pwr_on = false;
static bool force_pwr_on = false;  // trueにすると、X68Kの電源ONを強制的に検出したことにする
static uint32_t last_indexlow_ms = 0;

const int GP_UNIT = 2;  // GreenPAK3を使う

/**
 * @brief X68Kの電源がONになったことを検出したかどうか
 * ● OFF状態から、ONになったことの検出方法
 * INDEX信号がX68000側でプルアップされることを利用し、
 * GreenPAKの INDEX出力端子の入力状態をチェックすることで電源ONになったと判定する。
 * INDEX端子は、GreenPAK3の Matrix Input 11 (IO12 Digital Input) に接続されている。
 * この端子は Open-Drain 出力なので、X68K側でプルアップされている場合はHighになるため、
 * HighになったらON状態になったと判断します。ただ、当然ながらドライブがINDEX信号を出している
 * 瞬間はLowになるため、Lowになったからと言って即座にOFF状態になったとは判断せず、
 * 以下のようにします。
 * - INDEX端子の入力値をINDEX_IN、出力値をINDEX_OUTとする
 * - INDEX信号は直接入力端子の値を読まずに、(!INDEX_OUT or !INDEX_IN) の論理式の値を読みます
 *  - この値を INDEX_SNS と呼ぶことにします
 *  - こうすることで、INDEX_OUTがLowのとき、つまりINDEXを出力中は必ずINDEX_SNSがHighになります
 *  - INDEX_OUTがHighのとき、つまりINDEXを出力していないときは、INDEX_INの値がそのまま反映されます
 *  - 結果、INDEX_SNSを使うと「出力していないのでプルアップされて1になるはずなのに0になっている」
 *    ということが検出できます
 * - さらに、他のドライブがINDEXをLowにしているケースを考慮するために、D-FFを使用します
 *  - INDEX_SNSがLowであることを検出したら、D-FFをクリアします
 *  - このD-FFはINDEX_INの立ち上がりエッジで1がセットされるようになっているため、このまま少し待つと
 *    INDEXがディアクティベートされたタイミングでD-FFがセットされます
 *  - これを1-2秒後くらいに検出することで、「一時的にINDEXがLowになっただけ」というケースを除外できます
 * このINDEX_SNSはGP3のMatrix Input 18 (LUT3_0_DFF3_OUT) から読み取れます。
 */
static bool detect_x68k_power_on(uint32_t systick_ms) {
    if (force_pwr_on) {
        return true;
    }
    bool index_sns = greenpak_get_matrixinput(GP_UNIT, 18);  // index_sns = !index_out || !index_in
    // INDEX_SNSが HighならON状態になったと判断する
    return index_sns;
}

static bool detect_x68k_power_off(uint32_t systick_ms) {
    if (force_pwr_on) {
        return false;
    }
    if (last_indexlow_ms != 0) {
        if (systick_ms < last_indexlow_ms + 5000) {
            return false;  // 5000msec待つ
        }
        last_indexlow_ms = 0;
        // 5000msec経過したので、D-FF (D-FF 16)の状態をチェックする
        bool dffq = greenpak_get_matrixinput(GP_UNIT, 46) ? 0x01 : 0x00;
        if (dffq) {
            // D-FFがセットされたままなのでON状態が継続していると判断する
            return false;
        }
        // D-FFがクリアされたままなのでOFF状態になったと判断する
        return true;
    }
    // まずは、D-FFをクリアするために D-FFの nRESET につながっている Virtual Input7 (Bit0) を 1→0→1 にする
    uint8_t vin = greenpak_get_virtualinput(GP_UNIT);
    uint8_t vin0 = vin & ~(1 << 0);
    uint8_t vin1 = vin0 | (1 << 0);
    greenpak_set_virtualinput(GP_UNIT, vin1);
    greenpak_set_virtualinput(GP_UNIT, vin0);
    greenpak_set_virtualinput(GP_UNIT, vin1);
    // INDEX_SNS (Matrix Input 18) をチェックする
    bool index_sns = greenpak_get_matrixinput(GP_UNIT, 18);
    if (index_sns) {
        // HighなのでON状態が継続していると判断し判定終了
        last_indexlow_ms = 0;
        return false;
    }
    // LowなのでOFF状態になった可能性があるので、少し待ってからD-FFの状態をチェックする
    last_indexlow_ms = systick_ms;
    return false;
}

void power_control_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    static uint32_t last_systick_ms = 0;
    static uint32_t last_pvd_check_ms = 0;

    // PVDOフラグを定期的にチェック（割り込みが動作しないため、ポーリングで対応）
    // 500msごとにチェックして、低電圧を素早く検出
    if (systick_ms - last_pvd_check_ms >= 500) {
        last_pvd_check_ms = systick_ms;
        uint32_t csr = PWR->CSR;
        bool pvdo = (csr & (1 << 2)) != 0;
        static bool last_pvdo = false;
        if (pvdo != last_pvdo) {
            ui_logf(UI_LOG_LEVEL_WARN, "PVDO changed: %d->%d (CSR=0x%02x)\n", last_pvdo, pvdo, csr);
            last_pvdo = pvdo;
            // PVDOがセット（低電圧検出）されたらリセット処理
            if (pvdo) {
                ui_log(UI_LOG_LEVEL_ERROR, "Low voltage detected, resetting...\n");
                power_enter_sleep(ctx);
            }
        }
    }

    // 初回呼び出し時（last_systick_ms == 0）は必ずチェックする
    if (last_systick_ms != 0 && systick_ms - last_systick_ms < 500) {
        return;
    }
    last_systick_ms = systick_ms;

    // X68Kの電源が入っているかどうかをチェックする
    // ● ON状態から、OFFになったことの検出方法
    // たまたまINDEX信号がLowになっただけの可能性もあるので、以下の方法を用いる。
    // * まず、I2C経由で、GreenPAKのVirtual Input Registerをセットして内部のD-FFをクリアしておく
    // * 其の直後にINDEX_OUT信号の入力を検査して、HighならON状態が継続していると判断し判定終了
    // * Lowだった場合は、、500msec後にD-FFの出力を読み、クリアされたままならOFF状態になったと判断する
    // * D-FF (7) の出力は Matrix Input 46に接続されている

    ui_cursor(UI_PAGE_DEBUG, 0, 0);
    if (is_x68k_pwr_on) {
        // ON状態の時は、OFF状態になったかどうかをチェックする
        if (detect_x68k_power_off(systick_ms)) {
            // OFF状態になったことを検出した
            is_x68k_pwr_on = false;
            power_state.x68k_power_on = false;
            ctx->power_on = false;  // 起動ステータスを保存しておく
            ui_print(UI_PAGE_DEBUG, "X68K PWR OFF\n");
            last_indexlow_ms = 0;
            enable_fdd_power(ctx, false);  // FDDの電源をOFFにする
            // TODO 本来このタイミングではないが、ここでLOCK_REQUESTをかけておく
            GPIOC->BSHR = (1 << 6);  // LOCK_REQUEST = ON
            return;
        }
    }
    if (!is_x68k_pwr_on) {
        // OFF状態の時は、ON状態になったかどうかをチェックする
        if (detect_x68k_power_on(systick_ms)) {
            is_x68k_pwr_on = true;
            power_state.x68k_power_on = true;
            ctx->power_on = true;  // 起動ステータスを保存しておく
            ui_print(UI_PAGE_DEBUG, "X68K PWR ON \n");
            last_indexlow_ms = 0;

            // INA3221で+12V電圧をチェック（ch2: +12V_LINE）
            uint16_t ch1_current, ch1_voltage, ch2_current, ch2_voltage, ch3_current, ch3_voltage;
            ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);

            // +12Vが11V以下なら再ネゴシエーション
            if (ch2_voltage < 11000) {
                ui_logf(UI_LOG_LEVEL_WARN, "+12V low (%dmV), renegotiate\n", ch2_voltage);
                power_renegotiate_pd(ctx);
            }

            enable_fdd_power(ctx, true);  // FDDの電源をONにする
            // TODO 本来このタイミングではないが、ここでLOCK_REQUESTを開放しておく
            GPIOC->BCR = (1 << 6);  // LOCK_REQUEST 解放
            return;
        }
    }

    ui_cursor(UI_PAGE_DEBUG, 0, 1);
    for (int i = 0; i < 16; i++) {
        bool val = greenpak_get_matrixinput(GP_UNIT, i);
        if (i % 4 == 0) ui_write(UI_PAGE_DEBUG, ' ');
        ui_write(UI_PAGE_DEBUG, val ? '1' : '0');
    }
}

void set_force_pwr_on(bool enable) {
    force_pwr_on = enable;
}

/**
 * @brief PVD（電源電圧検出）の初期化
 *
 * CH32X035のPVD機能を有効化し、低電圧（約3V以下）を検出できるようにする。
 * PVD閾値は PWR_CTLR の PLS[2:0] ビットで設定する。
 *
 * PLS設定値と閾値の対応（CH32X035データシートより）:
 *   000: 2.0V
 *   001: 2.2V
 *   010: 2.4V
 *   011: 2.6V
 *   100: 2.8V
 *   101: 3.0V
 *   110: 3.2V
 *   111: 3.4V
 *
 * VDDが2.9V以下を検出したいので、PLS=100 (2.8V) を使用する。
 * PVDOフラグはVDD < V_PVD のときにセットされる。
 * PVDには約100mVのヒステリシスがあるため、2.8V設定時：
 *   - 電圧下降: 2.8V以下で検出（PVDO=1）
 *   - 電圧上昇: 2.9V程度で解除（PVDO=0）
 */
void power_pvd_init(void) {
    // PWRクロックを有効化
    RCC->APB1PCENR |= RCC_PWREN;

    // PWR_CTLRレジスタの設定
    // - PVDE (bit 4): PVDを有効化
    // - PLS[2:0] (bits 7-5): 閾値レベル設定 = 111 (3.4V)
    // PVDには内部ヒステリシスがあり、約100mVのマージンがある
    // 3.4V設定時: 下降時3.4V以下で検出、上昇時3.5V程度で解除
    uint32_t ctlr = PWR->CTLR;
    ctlr &= ~(0x7 << 5);  // PLS[2:0]をクリア
    ctlr |= (0x7 << 5);   // PLS = 111 (3.4V)
    ctlr |= (1 << 4);     // PVDE = 1 (PVD有効化)
    PWR->CTLR = ctlr;

    power_state.pvd_enabled = true;
    power_state.low_voltage_detected = false;

    // 初期化後の状態を確認
    uint32_t csr = PWR->CSR;
    bool pvdo = (csr & (1 << 2)) != 0;  // PVDO: bit 2
    ui_logf(UI_LOG_LEVEL_INFO, "PVD init (3.4V, polling mode) PVDO=%d\n", pvdo);
}

/**
 * @brief 低電圧検出時の待機処理
 *
 * 低電圧検出時に呼ばれる。FDD電源を安全にOFFにし、電圧が回復するまで待機する。
 * 電圧が4V以上に回復したらリセットして起動処理をやり直す。
 */
void power_enter_sleep(minyasx_context_t* ctx) {
    // FDD電源を緊急OFFにする
    GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)
    GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
    GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
    power_state.fdd_power_on = false;

    // UI_PAGE_BOOTを使って警告メッセージを表示
    ui_page_type_t page = UI_PAGE_BOOT;
    ui_clear(page);
    ui_cursor(page, 0, 2);
    ui_print(page, "!!!! LOW VOLTAGE !!!!");
    ui_cursor(page, 0, 4);
    ui_print(page, "Waiting for recovery");
    ui_cursor(page, 0, 5);
    ui_print(page, "     (>4.0V)");
    ui_change_page(UI_PAGE_BOOT);

    ui_log(UI_LOG_LEVEL_ERROR, "!!! LOW VOLTAGE !!!\n");
    ui_log(UI_LOG_LEVEL_ERROR, "Waiting for voltage recovery (>4V)...\n");

    // 電圧が回復するまで待機ループ
    while (1) {
        // INA3221で実際の電圧を測定（チャネル1: VBUS）
        uint16_t ch1_current, ch1_voltage;
        uint16_t ch2_current, ch2_voltage;
        uint16_t ch3_current, ch3_voltage;
        ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);

        // ch1_voltageはmV単位なので、4000mV (4V)以上で回復と判定
        if (ch1_voltage >= 4000) {
            // 電圧が4V以上に回復
            ui_logf(UI_LOG_LEVEL_INFO, "Voltage recovered to %dmV! Resetting...\n", ch1_voltage);

            // 少し待ってからリセット
            Delay_Ms(100);

            // システムリセットして起動処理をやり直す
            NVIC_SystemReset();
        }

        // 1秒ごとにログとOLED出力（生存確認）
        static uint32_t last_log_ms = 0;
        uint64_t systick = SysTick->CNT;
        uint32_t ms = systick / (F_CPU / 1000);
        if (ms - last_log_ms >= 3000) {
            last_log_ms = ms;
            ui_logf(UI_LOG_LEVEL_WARN, "Waiting.. VBUS=%dmV\n", ch1_voltage);

            // UI_PAGE_BOOTに現在の電圧を表示
            ui_page_type_t page = UI_PAGE_BOOT;
            ui_cursor(page, 0, 7);
            ui_printf(page, "VBUS: %d.%03dV         ", ch1_voltage / 1000, ch1_voltage % 1000);
        }

        // 少し待つ
        Delay_Ms(100);
    }
}

/**
 * @brief 電源状態の取得
 *
 * @return 電源状態構造体へのポインタ
 */
power_state_t* power_get_state(void) {
    return &power_state;
}

/**
 * @brief USB-PDネゴシエーションの再実行
 *
 * X68000電源ON検出時に+12V電圧が不足している場合に呼ばれる。
 * USB-PDで+12Vを再ネゴシエーションし、それでも無理なら+5Vにフォールバックする。
 *
 * @param ctx コンテキスト
 * @return true: ネゴシエーション成功（+12Vまたは+5V有効化）, false: 失敗
 */
bool power_renegotiate_pd(minyasx_context_t* ctx) {
    ui_log(UI_LOG_LEVEL_INFO, "Renegotiate USB-PD\n");

    // 一旦全てのFDD電源をOFFにする
    GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)
    GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
    GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)

    // 外部+12V電源をチェック
    if ((GPIOA->INDR & (1 << 17)) == 0) {
        // 外部+12V電源が接続されている
        GPIOA->BSXR = (1 << (20 - 16));  // Enable (+12V_EXT_EN=High)
        ui_log(UI_LOG_LEVEL_INFO, "+12V_EXT active\n");
        return true;
    }

    // USB-PDで+12Vを試みる
    bool pd_12v_enabled = activate_pd_12v(ctx);
    if (pd_12v_enabled) {
        ui_log(UI_LOG_LEVEL_INFO, "PD 12V OK\n");
        return true;
    }

    // +12Vが無理なら+5Vにフォールバック
    ui_log(UI_LOG_LEVEL_WARN, "PD 12V failed, fallback to 5V\n");

    // INA3221でVBUS(ch1)の電圧をチェック
    uint16_t ch1_current, ch1_voltage, ch2_current, ch2_voltage, ch3_current, ch3_voltage;
    ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);

    if (ch1_voltage >= 4750 && ch1_voltage <= 5500) {
        ui_logf(UI_LOG_LEVEL_INFO, "VBUS 5V OK (%dmV)\n", ch1_voltage);
        GPIOA->BSXR = (1 << (18 - 16));  // Enable (+5V_EN=High)
        return true;
    } else {
        ui_logf(UI_LOG_LEVEL_ERROR, "VBUS voltage NG (%dmV)\n", ch1_voltage);
        return false;
    }
}