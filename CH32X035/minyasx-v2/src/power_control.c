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

#include "power_control.h"

#include "greenpak/greenpak_control.h"
#include "ina3221/ina3221_control.h"
#include "ui/ui_control.h"
#include "usbpd/usbpd_sink.h"

static bool fdd_power_enabled = false;
void enable_fdd_power(minyasx_context_t* ctx, bool enable);

/**
 * @brief USB-PDで+12Vを要求して有効化する
 *     +12Vが有効化できたらtrueを返す
 *     +12Vが有効化できなかったらfalseを返す
 */
bool activate_pd_12v(minyasx_context_t* ctx) {
    if (!PD_connect()) {
        ui_print(UI_PAGE_DEBUG, "USBPD_Init error");
        ui_write(UI_PAGE_DEBUG, '\n');
        Delay_Ms(1000);
        return false;
    }
    ui_print(UI_PAGE_DEBUG, "USBPD_Init OK");
    ui_write(UI_PAGE_DEBUG, '\n');
    ctx->usbpd.connected = true;
    ctx->usbpd.pdonum = PD_getPDONum();

    int pd_12v_pdo = -1;
    for (int i = 1; i <= PD_getPDONum(); i++) {
        if (i <= PD_getFixedNum()) {
            ui_printf(UI_PAGE_DEBUG, " (%d)%6dmV %5dmA ", i, PD_getPDOVoltage(i), PD_getPDOMaxCurrent(i));
            if (PD_getPDOVoltage(i) == 12000 && PD_getPDOMaxCurrent(i) >= 1600) {
                // 12V, 1.6A以上のPDOを発見
                pd_12v_pdo = i;
            }
            // USB-PD情報をコンテキストに保存
            if (i < 8) {
                ctx->usbpd.pod[i - 1].voltage_mv = PD_getPDOVoltage(i);
                ctx->usbpd.pod[i - 1].current_ma = PD_getPDOMaxCurrent(i);
            }
        } else {
            ui_printf(UI_PAGE_DEBUG, " [%d]%6dmV-%5dmV ", i, PD_getPDOMinVoltage(i), PD_getPDOMaxVoltage(i));
        }
    }

    if (pd_12v_pdo >= 0) {
        ui_print(UI_PAGE_DEBUG, " -> request 12V");
        ui_write(UI_PAGE_DEBUG, '\n');
        if (PD_setPDO(pd_12v_pdo, 12000)) {
            ui_print(UI_PAGE_DEBUG, " -> 12V OK");
            ui_write(UI_PAGE_DEBUG, '\n');
            // +12Vに切り替わったことを確認できたら、+12VラインをEnableします。
            GPIOA->BSXR = (1 << (19 - 16));  // Enable (+12V_EN=High)
            return true;
        } else {
            ui_print(UI_PAGE_DEBUG, " -> 12V NG");
            ui_write(UI_PAGE_DEBUG, '\n');
            pd_12v_pdo = -1;
        }
    } else {
        ui_print(UI_PAGE_DEBUG, " -> no 12V PDO");
        ui_write(UI_PAGE_DEBUG, '\n');
    }
    return false;
}

void power_control_init(minyasx_context_t* ctx) {
    ctx->usbpd.connected = false;

    // ● 0.+12V電源ライン、+5V電源ラインのDisable
    GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
    GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
    GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)

    //
    Delay_Ms(1000);
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
            ui_print(UI_PAGE_DEBUG, "+12V_EXT_DET active");
            ui_write(UI_PAGE_DEBUG, '\n');
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
                    ui_print(UI_PAGE_DEBUG, "VBUS 5V OK");
                    ui_write(UI_PAGE_DEBUG, '\n');
                    GPIOA->BSXR = (1 << (18 - 16));  // Enable (+5V_EN=High)
                } else {
                    ui_printf(UI_PAGE_DEBUG, "VBUS 5V NG %dmV", ch1_voltage);
                    ui_write(UI_PAGE_DEBUG, '\n');
                }
            }
        }
        fdd_power_enabled = true;
    } else {
        // FDDの電源OFF
        GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)
        GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
        GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
        fdd_power_enabled = false;
    }
}

bool fdd_power_is_enabled(void) {
    return fdd_power_enabled;
}

static bool is_x68k_pwr_on = false;
static uint32_t last_indexlow_ms = 0;

const int GP_UNIT = 2;  // GreenPAK3を使う

void power_control_poll(minyasx_context_t* ctx, uint32_t systick_ms) {
    static uint32_t last_systick_ms = 0;
    if (systick_ms - last_systick_ms < 500) {
        return;
    }
    last_systick_ms = systick_ms;

    // X68Kの電源が入っているかどうかをチェックする
    // ● OFF状態から、ONになったことの検出方法
    // INDEX信号がX68000側でプルアップされることを利用し、
    // GreenPAKの INDEX_OUTの端子の状態をチェックすることで電源ONになったと判定する。
    // INDEX_OUT端子の入力は、GreenPAK2の Matrix Input 11 (IO12 Digital Input) に接続されている。
    // ● ON状態から、OFFになったことの検出方法
    // たまたまINDEX信号がLowになっただけの可能性もあるので、以下の方法を用いる。
    // * まず、I2C経由で、GreenPAKのVirtual Input Registerをセットして内部のD-FFをクリアしておく
    // * 其の直後にINDEX_OUT信号の入力を検査して、HighならON状態が継続していると判断し判定終了
    // * Lowだった場合は、、500msec後にD-FFの出力を読み、クリアされたままならOFF状態になったと判断する
    // * D-FF (7) の出力は Matrix Input 46に接続されている

    ui_cursor(UI_PAGE_DEBUG, 0, 1);
    if (last_indexlow_ms != 0) {
        if (systick_ms < last_indexlow_ms + 500) {
            return;  // 500msec待つ
        }
        // 500msec経過したので、D-FFの状態をチェックする
        bool dffq = greenpak_get_matrixinput(GP_UNIT, 46) ? 0x01 : 0x00;
        if (dffq) {
            // D-FFがセットされたままなのでON状態が継続していると判断する
            last_indexlow_ms = 0;
            return;
        }
        // D-FFがクリアされたままなのでOFF状態になったと判断する
        is_x68k_pwr_on = false;
        ui_print(UI_PAGE_DEBUG, "X68K PWR OFF\n");
        last_indexlow_ms = 0;
        enable_fdd_power(ctx, false);  // FDDの電源をOFFにする
        // TODO 本来このタイミングではないが、ここでGP_ENABLEをOFFにしておく
        GPIOC->BCR = (1 << 6);  // GP_ENABLE = OFF
        return;
    } else if (is_x68k_pwr_on) {
        // ON状態の時は、OFF状態になったかどうかをチェックする
        // まずは、D-FFをクリアするために Bit0(Virtual Input7) を 0→1→0 にする
        uint8_t vin = greenpak_get_virtualinput(GP_UNIT);
        uint8_t vin0 = vin & ~(1 << 0);
        uint8_t vin1 = vin0 | (1 << 0);
        greenpak_set_virtualinput(GP_UNIT, vin0);
        greenpak_set_virtualinput(GP_UNIT, vin1);
        greenpak_set_virtualinput(GP_UNIT, vin0);
        // Matrix Input 11 (IO12 Digital Input) をチェックする
        bool index_state = greenpak_get_matrixinput(GP_UNIT, 11);
        if (index_state) {
            // HighなのでON状態が継続していると判断し判定終了
            last_indexlow_ms = 0;
            return;
        }
        last_indexlow_ms = systick_ms;
    }
    if (!is_x68k_pwr_on) {
        // OFF状態の時は、ON状態になったかどうかをチェックする
        bool index_state = greenpak_get_matrixinput(GP_UNIT, 11);
        if (index_state) {
            // HighなのでON状態になったと判断する
            is_x68k_pwr_on = true;
            ui_print(UI_PAGE_DEBUG, "X68K PWR ON \n");
            last_indexlow_ms = 0;
            enable_fdd_power(ctx, true);  // FDDの電源をONにする
            // TODO 本来このタイミングではないが、ここでGP_ENABLEをONにしておく
            GPIOC->BSHR = (1 << 6);  // GP_ENABLE = ON
            return;
        }
    }

    ui_cursor(UI_PAGE_DEBUG, 0, 2);
    for (int i = 0; i < 16; i++) {
        bool val = greenpak_get_matrixinput(GP_UNIT, i);
        if (i % 4 == 0) ui_write(UI_PAGE_DEBUG, ' ');
        ui_write(UI_PAGE_DEBUG, val ? '1' : '0');
    }
}
