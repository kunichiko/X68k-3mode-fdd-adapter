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

#include "usbpd/usbpd_sink.h"
#include "oled/ssd1306_txt.h"

void power_control_init(void)
{
    OLED_clear();

    // INA3221の初期化
    //    ina3221_init();

    // ● 0.+12V電源ライン、+5V電源ラインのDisable
    // GPIOのマッピングは以下の通り
    // PA17: +12V_EXT_DET (Low=外部+12V電源接続, Pull-Up)
    // PA18: +5V_EN (Low=Enable, High=Disable)
    // PA19: +12V_EN (Low=Enable, High=Disable)
    // PA20: +12V_EXT_EN (Low=Enable, High=Disable))
    GPIOA->BSXR = (1 << (18 - 16)); // Disable (+5V_EN=High)
    GPIOA->BSXR = (1 << (19 - 16)); // Disable (+12V_EN=High)
    GPIOA->BSXR = (1 << (20 - 16)); // Disable (+12V_EXT_EN=High)

    // ● 1.外部+12V電源の検出
    // 外部+12V電源が接続されていれば、+12V_EXT_DETがLowになります。
    // この場合は、+12V_EXT ラインをEnableします。
    if ((GPIOA->INDR & (1 << 17)) == 0)
    {
        // 外部+12V電源が接続されている
        GPIOA->BCR = (1 << 20); // Enable (+12V_EXT_EN=Low)
        OLED_print("+12V_EXT_DET active");
        OLED_write('\n');
    }
    else
    {
        // 外部+12V電源が接続されていない
        // ● 2.USB-PDのネゴシエーション
        // 外部電源がない場合は、PD電源から +12V, 1.6A以上の供給が可能かどうかを調べます。

        if (!PD_connect())
        {
            OLED_print("USBPD_Init error");
            OLED_write('\n');
            return;
        }
        OLED_print("USBPD_Init OK");
        OLED_write('\n');

        for (int i = 1; i <= PD_getPDONum(); i++)
        {
            if (i <= PD_getFixedNum())
                OLED_printf(" (%d)%6dmV %5dmA ", i, PD_getPDOVoltage(i), PD_getPDOMaxCurrent(i));
            else
                OLED_printf(" [%d]%6dmV-%5dmV ", i, PD_getPDOMinVoltage(i), PD_getPDOMaxVoltage(i));
        }

        Delay_Ms(3000);
    }
}

void power_control_poll(uint32_t systick_ms)
{
    // 定期的な処理をここに追加
}
