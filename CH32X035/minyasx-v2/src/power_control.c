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

#include "ina3221/ina3221_control.h"
#include "ui/ui_control.h"
#include "usbpd/usbpd_sink.h"

/**
 * @brief USB-PDで+12Vを要求して有効化する
 *     +12Vが有効化できたらtrueを返す
 *     +12Vが有効化できなかったらfalseを返す
 */
bool activate_pd_12v() {
    if (!PD_connect()) {
        ui_print(UI_PAGE_MAIN, "USBPD_Init error");
        ui_write(UI_PAGE_MAIN, '\n');
        Delay_Ms(1000);
        return false;
    }
    ui_print(UI_PAGE_MAIN, "USBPD_Init OK");
    ui_write(UI_PAGE_MAIN, '\n');

    int pd_12v_pdo = -1;
    for (int i = 1; i <= PD_getPDONum(); i++) {
        if (i <= PD_getFixedNum()) {
            ui_printf(UI_PAGE_MAIN, " (%d)%6dmV %5dmA ", i, PD_getPDOVoltage(i), PD_getPDOMaxCurrent(i));
            //         if (PD_getPDOVoltage(i) == 9000 && PD_getPDOMaxCurrent(i) >= 1600)
            if (PD_getPDOVoltage(i) == 12000 && PD_getPDOMaxCurrent(i) >= 1600)  // テストように9000mVにしている
            {
                pd_12v_pdo = i;
            }
        } else {
            ui_printf(UI_PAGE_MAIN, " [%d]%6dmV-%5dmV ", i, PD_getPDOMinVoltage(i), PD_getPDOMaxVoltage(i));
        }
    }

    if (pd_12v_pdo >= 0) {
        ui_print(UI_PAGE_MAIN, " -> request 12V");
        ui_write(UI_PAGE_MAIN, '\n');
        if (PD_setPDO(pd_12v_pdo, 12000)) {
            ui_print(UI_PAGE_MAIN, " -> 12V OK");
            ui_write(UI_PAGE_MAIN, '\n');
            // +12Vに切り替わったことを確認できたら、+12VラインをEnableします。
            GPIOA->BSXR = (1 << (19 - 16));  // Enable (+12V_EN=High)
            return true;
        } else {
            ui_print(UI_PAGE_MAIN, " -> 12V NG");
            ui_write(UI_PAGE_MAIN, '\n');
            pd_12v_pdo = -1;
        }
    } else {
        ui_print(UI_PAGE_MAIN, " -> no 12V PDO");
        ui_write(UI_PAGE_MAIN, '\n');
    }
    return false;
}

void power_control_init(void) {
    ui_clear(UI_PAGE_MAIN);

    // ● 0.+12V電源ライン、+5V電源ラインのDisable
    // GPIOのマッピングは以下の通り
    // PA17: +12V_EXT_DET (Low=外部+12V電源接続, Pull-Up)
    // PA18: +5V_EN (Low=Disable, High=Enable)
    // PA19: +12V_EN (Low=Disable, High=Enable)
    // PA20: +12V_EXT_EN (Low=Disable, High=Enable))
    GPIOA->BCR = (1 << (18));  // Disable (+5V_EN=Low)
    GPIOA->BCR = (1 << (19));  // Disable (+12V_EN=Low)
    GPIOA->BCR = (1 << (20));  // Disable (+12V_EXT_EN=Low)

    // ● 1.外部+12V電源の検出
    // 外部+12V電源が接続されていれば、+12V_EXT_DETがLowになります。
    // この場合は、+12V_EXT ラインをEnableします。
    if ((GPIOA->INDR & (1 << 17)) == 0) {
        // 外部+12V電源が接続されている
        GPIOA->BSXR = (1 << (20 - 16));  // Enable (+12V_EXT_EN=High)
        ui_print(UI_PAGE_MAIN, "+12V_EXT_DET active");
        ui_write(UI_PAGE_MAIN, '\n');
    } else {
        // 外部+12V電源が接続されていない
        // ● 2.USB-PDのネゴシエーション
        // 外部電源がない場合は、PD電源から +12V, 1.6A以上の供給が可能かどうかを調べます。
        bool pd_12v_enabled = activate_pd_12v();
        if (!pd_12v_enabled) {
            // +12Vが有効化できなかった場合は、VBUSが+5Vなので、+5VラインをEnableします。
            // 念の為、INA3221で VBUS(ch1)の電圧をチェックします。
            uint16_t ch1_current, ch1_voltage, ch2_current, ch2_voltage, ch3_current, ch3_voltage;
            ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);
            if (ch1_voltage >= 4750 && ch1_voltage <= 5500) {
                ui_print(UI_PAGE_MAIN, "VBUS 5V OK");
                ui_write(UI_PAGE_MAIN, '\n');
                GPIOA->BSXR = (1 << (18 - 16));  // Enable (+5V_EN=High)
            } else {
                ui_printf(UI_PAGE_MAIN, "VBUS 5V NG %dmV", ch1_voltage);
                ui_write(UI_PAGE_MAIN, '\n');
            }
        }
    }

    Delay_Ms(3000);
}

void power_control_poll(uint32_t systick_ms) {
    // 定期的な処理をここに追加
}
