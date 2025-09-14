#include <stdint.h>
#include <string.h>

#include "greenpak_auto.h"
#include "greenpak_program.h"

#include "i2c/i2c_ch32x035.h"
#include "oled/ssd1306_txt.h"

#include "greenpak/greenpak1.h"
#include "greenpak/greenpak2.h"
#include "greenpak/greenpak3.h"
#include "greenpak/greenpak4.h"

#define GP_DEF_NVM 0x0A // はじめは全ICが 0x08-0x0B (NVM=0x0A) に居る想定（基板上で1個ずつ接続）
#define GP_PAGE 16      // 1回の書込みチャンク（必要なら調整）
#define CMP_CHUNK 1     // 比較時の読出しチャンク

// 最終配置（NVMアドレスのみ並べる）
static const uint8_t gp_target_nvm[4] = {0x12, 0x1A, 0x22, 0x2A};

// 画像テーブル（サイズ0はスキップ）
typedef struct
{
    uint16_t base;
    uint16_t size;
    const uint8_t *image;
} gp_img_t;

static const gp_img_t gp_img[4] = {
    {GREENPAK1_BASE, GREENPAK1_SIZE, GREENPAK1_IMAGE},
    {GREENPAK2_BASE, GREENPAK2_SIZE, GREENPAK2_IMAGE},
    {GREENPAK3_BASE, GREENPAK3_SIZE, GREENPAK3_IMAGE},
    {GREENPAK4_BASE, GREENPAK4_SIZE, GREENPAK4_IMAGE},
};

// ---------------- I2Cユーティリティ ----------------

// 在否確認（ACKプローブ）。成功で1、失敗で0。
// 7bitアドレスを渡す（例: 0x0A）
// i2cdetect と同じ “SMBus Quick (write)” 方式の ACK プローブ
// 引数: 7bit アドレス（例: 0x0A）
// 戻り: 在席=1 / 不在=0
static int i2c_probe7(uint8_t addr7)
{
    // 1) BUSY解除待ち
    uint32_t to = I2C_TIMEOUT_MAX;
    while ((I2C1->STAR2 & I2C_STAR2_BUSY) && --to)
        ;
    if (!to)
    {
        I2C_error(0);
        return 0;
    }

    // 2) START
    I2C1->CTLR1 |= I2C_CTLR1_START;
    //    マスタモード突入 (SB/MSL/BUSY) を待つ
    if (I2C_wait_evt(I2C_EVENT_MASTER_MODE_SELECT, 1))
    {
        I2C1->CTLR1 |= I2C_CTLR1_STOP;
        return 0;
    }

    // 3) SLA+W 送出（ACK を見る）
    I2C1->DATAR = (addr7 << 1); // write方向

    //    ADDR(ACK) or AF(NACK) を待つ
    int present = 0;
    to = I2C_TIMEOUT_MAX;
    while (--to)
    {
        uint16_t sr1 = I2C1->STAR1;
        if (sr1 & I2C_STAR1_ADDR)
        {                      // ACK=在席
            (void)I2C1->STAR2; // ADDR クリア（SR1→SR2 読み）
            present = 1;
            break;
        }
        if (sr1 & I2C_STAR1_AF)
        { // NACK=不在
            present = 0;
            break;
        }
    }

    // 4) STOP（バス解放）
    I2C1->CTLR1 |= I2C_CTLR1_STOP;

    // 5) 後始末（NACK/Timeout 時は I2C を立て直す）
    if (!present || !to)
    {
        // AF はソフトクリアも可能だが、確実さ重視で全体リカバリ
        I2C_error(0);
    }
    return present;
}

// 連続書込み（ページ分割, STOPはwriteBufferが発行）
void gp_write_seq(uint8_t addr7, uint16_t start, const uint8_t *data, uint16_t len)
{
    uint16_t off = 0;
    while (off < len)
    {
        uint16_t n = len - off;
        if (n > GP_PAGE)
            n = GP_PAGE;

        I2C_start((addr7 << 1) | 0);               // START + SLA+W（8bitアドレス）:contentReference[oaicite:9]{index=9}
        I2C_write((uint8_t)(start + off));         // 内部アドレス（多くのGreenPAKはオートインクリメント）:contentReference[oaicite:10]{index=10}
        I2C_writeBuffer((uint8_t *)&data[off], n); // データ送出→STOPまで:contentReference[oaicite:11]{index=11}

        off += n;
    }
}

// 連続読出し（STOPはreadBufferが発行）
static void gp_read_seq(uint8_t addr7, uint16_t start, uint8_t *dst, uint16_t len)
{
    I2C_start((addr7 << 1) | 0); // 書きモードで内部アドレスをセット
    I2C_write((uint8_t)start);
    I2C_restart((addr7 << 1) | 1); // 再スタートして読出しへ切替:contentReference[oaicite:12]{index=12}
    I2C_readBuffer(dst, len);      // 連続受信→最後にSTOP:contentReference[oaicite:13]{index=13}
}

static int gp_compare_image(uint8_t addr7, uint16_t base, const uint8_t *img, uint16_t size)
{
    if (size == 0)
        return 1; // 空なら一致扱い
    uint8_t buf[CMP_CHUNK];
    uint16_t done = 0;
    while (done < size)
    {
        uint16_t n = size - done;
        if (n > CMP_CHUNK)
            n = CMP_CHUNK;
        gp_read_seq(addr7, base + done, buf, n);
        if (memcmp(buf, &img[done], n) != 0)
        {
            OLED_print("mismatch at ");
            OLED_printD(base + done);
            OLED_write('\n');
            return 0; // 不一致
        }
        done += n;
    }
    return 1; // 完全一致
}

// ---------------- 自動処理本体 ----------------

static void show_not_found_and_exit(void)
{
    OLED_clear();
    OLED_cursor(0, 0);
    OLED_print("GreenPAK not found");
    OLED_write('\n'); // :contentReference[oaicite:14]{index=14} :contentReference[oaicite:15]{index=15}
    // 必要なら、ここで他の処理へ遷移/return
}

void greenpak_autoprogram_verify(void)
{
    /*  OLED_write('\n');
        OLED_print("programming GP1 on GP2");
        gp_program_with_erase(gp_target_nvm[1], gp_img[0].base, gp_img[0].image, gp_img[0].size);
        OLED_print("done");
        return;*/

    OLED_clear();

    // まず既定の最終配置（0x12, 0x1A, 0x22, 0x2A）と 0x0A（作業用）をスキャン
    int present[4] = {
        i2c_probe7(gp_target_nvm[0]),
        i2c_probe7(gp_target_nvm[1]),
        i2c_probe7(gp_target_nvm[2]),
        i2c_probe7(gp_target_nvm[3]),
    };
    int def_present = i2c_probe7(GP_DEF_NVM);

    if (!present[0] && !present[1] && !present[2] && !present[3] && !def_present)
    {
        // どこにも居ない → 表示して終了
        show_not_found_and_exit();
        return;
    }

    OLED_cursor(0, 0);
    OLED_print("GP found ");
    if (present[0])
    {
        OLED_print("1 ");
    }
    if (present[1])
    {
        OLED_print("2 ");
    }
    if (present[2])
    {
        OLED_print("3 ");
    }
    if (present[3])
    {
        OLED_print("4 ");
    }
    if (def_present)
    {
        OLED_print("def ");
    }
    OLED_write('\n');

    // すべて見えている場合でも「差分があれば上書き」する
    for (;;)
    {
        // 全員が最終番地に居るか？
        int all_seen = present[0] && present[1] && present[2] && present[3];

        // 居るものは verify→差分があれば上書き（最終番地側へ書く）
        for (int i = 0; i < 4; i++)
        {
            if (present[i] && gp_img[i].size > 0)
            {
                int same = gp_compare_image(gp_target_nvm[i], gp_img[i].base, gp_img[i].image, gp_img[i].size - 0x10);
                if (same)
                {
                    OLED_print("firm is ok:");
                    OLED_printD(i + 1);
                    OLED_write('\n');
                }
                else
                {
                    OLED_print("reprogramming:");
                    OLED_printD(i + 1);
                    OLED_write('\n');
                    gp_program_with_erase(gp_target_nvm[i], gp_img[i].base, gp_img[i].image, gp_img[i].size);
                    OLED_print("done");
                    OLED_write('\n');
                }
            }
        }

        if (all_seen)
        {
            // 全員在席＋必要なら上書き済み → 完了
            return;
        }

        // まだ見えていない最初のICを 0x0A（作業用）経由でプログラム
        // （基板のショートジャンパで、そのICだけがバスに接続されている前提）
        int target = -1;
        for (int i = 0; i < 4; i++)
        {
            if (!present[i])
            {
                target = i;
                break;
            }
        }

        if (target < 0)
        {
            // 論理上起きないはずだが保険
            return;
        }

        if (!i2c_probe7(GP_DEF_NVM))
        {
            // 作業用 0x0A 自体が見えない → 表示して終了
            show_not_found_and_exit();
            return;
        }

        if (gp_img[target].size > 0)
        {
            OLED_print("programming GP");
            OLED_printD(target + 1);
            OLED_write('\n');
            // 0x0A 側へ書き込む（書込み完了でICが自動的に再配置される前提）
            gp_program_with_erase(GP_DEF_NVM, gp_img[target].base, gp_img[target].image, gp_img[target].size);
        }
        else
        {
            OLED_print("data empty: Skip GP");
            OLED_printD(target + 1);
            OLED_write('\n');
        }

        // 再スキャン（当該ICが最終番地に現れるはず）
        present[target] = i2c_probe7(gp_target_nvm[target]);
        // 次ループへ
    }
}
