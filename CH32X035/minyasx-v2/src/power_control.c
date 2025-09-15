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

#include "usbpd/usbpd.h"
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

        // Change to eUSBPD_VCC_3V3 if you are powering your board from a 3.3V source
        // USBPD_VCC_e vcc = eUSBPD_VCC_3V3;
        USBPD_VCC_e vcc = eUSBPD_VCC_5V0;
        USBPD_Result_e result = USBPD_Init(vcc);
        if (eUSBPD_OK != result)
        {
            OLED_print("USBPD_Init error");
            OLED_write('\n');
            return;
        }
        OLED_print("USBPD_Init OK");
        OLED_write('\n');

        USBPD_Reset();

        uint32_t start = SysTick->CNT;
        uint32_t lastLog = 0;
        while (eUSBPD_BUSY == (result = USBPD_SinkNegotiate()))
        {
            // ネゴシエーション中
            if (SysTick->CNT > start + (F_CPU * 10))
            {
                // 10秒以上かかったらタイムアウト
                OLED_print("USBPD_SinkNegotiate timeout");
                OLED_write('\n');
                break;
            }
        }
        //
        if (eUSBPD_OK != result)
        {
            OLED_printf("USB PD negotiation failed:\n");
            Delay_Ms(1000);
            OLED_printf("%s, state: %s", USBPD_ResultToStr(result),
                        USBPD_StateToStr(USBPD_GetState()));
            OLED_write('\n');
        }
        else
        {
            int ver = USBPD_GetVersion();
            OLED_printf("USB PD V%d.0 negotiation done", ver);
            // OLED_printf("USB PD V%d.0 negotiation done", USBPD_GetVersion());
            OLED_write('\n');
            Delay_Ms(1000);
            // 利用できる電圧一覧を取得
            USBPD_SPR_CapabilitiesMessage_t *capabilities;
            const size_t count = USBPD_GetCapabilities(&capabilities);

            OLED_print("USB PD capabilities:\n");
            Delay_Ms(1000);
            for (size_t i = 0; i < count; i++)
            {
                Delay_Ms(1000);
                const USBPD_SourcePDO_t *pdo = &capabilities->Source[i];
                switch (pdo->Header.PDOType)
                {
                case eUSBPD_PDO_FIXED:
                    OLED_printf("%d: " FIXED_SUPPLY_FMT, i, FIXED_SUPPLY_FMT_ARGS(pdo));
                    break;
                case eUSBPD_PDO_BATTERY:
                    OLED_printf("%d: " BATTERY_SUPPLY_FMT, i, BATTERY_SUPPLY_FMT_ARGS(pdo));
                    break;
                case eUSBPD_PDO_VARIABLE:
                    OLED_printf("%d: " VARIABLE_SUPPLY_FMT, i, VARIABLE_SUPPLY_FMT_ARGS(pdo));
                    break;
                case eUSBPD_PDO_AUGMENTED:
                    switch (pdo->Header.AugmentedType)
                    {
                    case eUSBPD_APDO_SPR_PPS:
                        OLED_printf("%d: " SPR_PPS_FMT, i, SPR_PPS_FMT_ARGS(pdo));
                        break;
                    case eUSBPD_APDO_SPR_AVS:
                        OLED_printf("%d: " SPR_AVS_FMT, i, SPR_AVS_FMT_ARGS(pdo));
                        break;
                    case eUSBPD_APDO_EPR_AVS:
                        OLED_printf("%d: " EPR_AVS_FMT, i, EPR_AVS_FMT_ARGS(pdo));
                        break;
                    default:
                        OLED_printf("  Unknown Augmented PDO type: %d", pdo->Header.AugmentedType);
                        break;
                    }
                    break;
                default:
                    OLED_printf("  Unknown PDO type: %d", pdo->Header.PDOType);
                    break;
                }
            }
        }
    }
}

void power_control_poll(uint32_t systick_ms)
{
    // 定期的な処理をここに追加
}
