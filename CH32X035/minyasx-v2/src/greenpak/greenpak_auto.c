#include "greenpak_auto.h"

#include <stdint.h>
#include <string.h>

#include "greenpak/greenpak1.h"
#include "greenpak/greenpak2.h"
#include "greenpak/greenpak3.h"
#include "greenpak/greenpak4.h"
#include "greenpak/greenpak5.h"
#include "greenpak_control.h"
#include "greenpak_program.h"
#include "i2c/i2c_ch32x035.h"
#include "ui/ui_control.h"

#define GP_PAGE 16   // 1回の書込みチャンク（必要なら調整）
#define CMP_CHUNK 1  // 比較時の読出しチャンク

// 画像テーブル（サイズ0はスキップ）
typedef struct {
    uint16_t base;
    uint16_t size;
    const uint8_t *image;
} gp_img_t;

static const gp_img_t gp_img[GP_NUM] = {
    {GREENPAK1_BASE, GREENPAK1_SIZE, GREENPAK1_IMAGE},
    {GREENPAK2_BASE, GREENPAK2_SIZE, GREENPAK2_IMAGE},
    {GREENPAK3_BASE, GREENPAK3_SIZE, GREENPAK3_IMAGE},
    {GREENPAK4_BASE, GREENPAK4_SIZE, GREENPAK4_IMAGE},
    {GREENPAK5_BASE, GREENPAK5_SIZE, GREENPAK5_IMAGE},
};

// 連続書込み（ページ分割, STOPはwriteBufferが発行）
void gp_write_seq(uint8_t addr7, uint16_t start, const uint8_t *data, uint16_t len) {
    uint16_t off = 0;
    while (off < len) {
        uint16_t n = len - off;
        if (n > GP_PAGE) n = GP_PAGE;

        I2C_start((addr7 << 1) | 0);        // START + SLA+W（8bitアドレス）:contentReference[oaicite:9]{index=9}
        I2C_write((uint8_t)(start + off));  // 内部アドレス（多くのGreenPAKはオートインクリメント）:contentReference[oaicite:10]{index=10}
        I2C_writeBuffer((uint8_t *)&data[off], n);  // データ送出→STOPまで:contentReference[oaicite:11]{index=11}

        off += n;
    }
}

// 連続読出し（STOPはreadBufferが発行）
static void gp_read_seq(uint8_t nvm_addr7, uint16_t start, uint8_t *dst, uint16_t len) {
    I2C_start((nvm_addr7 << 1) | 0);  // 書きモードで内部アドレスをセット
    I2C_write((uint8_t)start);
    I2C_restart((nvm_addr7 << 1) | 1);  // 再スタートして読出しへ切替:contentReference[oaicite:12]{index=12}
    I2C_readBuffer(dst, len);           // 連続受信→最後にSTOP:contentReference[oaicite:13]{index=13}
}

static int gp_compare_image(uint8_t addr7, uint16_t base, const uint8_t *img, uint16_t size) {
    if (size == 0) return 1;  // 空なら一致扱い

    uint8_t nvm_addr7 = (uint8_t)((addr7 & 0xfc) | 0x02);  // NVMアドレスに変換 (addrは0x08,0x10,0x18,0x20,0x28のいずれか)
    uint8_t buf[CMP_CHUNK];
    uint16_t done = 0;
    while (done < size) {
        uint16_t n = size - done;
        if (n > CMP_CHUNK) n = CMP_CHUNK;
        gp_read_seq(nvm_addr7, base + done, buf, n);
        if (memcmp(buf, &img[done], n) != 0) {
            ui_log(UI_LOG_LEVEL_INFO, "mismatch at ");
            ui_printD(UI_PAGE_LOG, base + done);
            ui_log(UI_LOG_LEVEL_INFO, "\n");
            return 0;  // 不一致
        }
        done += n;
    }
    return 1;  // 完全一致
}

// ---------------- 自動処理本体 ----------------

static void show_not_found_and_exit(void) {
    ui_log(UI_LOG_LEVEL_INFO, "GreenPAK not found");
    ui_log(UI_LOG_LEVEL_INFO, "\n");  // :contentReference[oaicite:14]{index=14} :contentReference[oaicite:15]{index=15}
    // 必要なら、ここで他の処理へ遷移/return
}

void greenpak_force_program_verify(uint8_t addr, uint8_t unit) {
    ui_log(UI_LOG_LEVEL_INFO, "prog GP");
    ui_printD(UI_PAGE_LOG, unit + 1);
    ui_log(UI_LOG_LEVEL_INFO, "  @0x");
    ui_printH(UI_PAGE_LOG, addr);
    ui_log(UI_LOG_LEVEL_INFO, "\n");
    gp_program_with_erase(addr, gp_img[unit].base, gp_img[unit].image, gp_img[unit].size);
    ui_log(UI_LOG_LEVEL_INFO, "done");
    return;
}

void greenpak_autoprogram_verify(void) {
    ui_clear(UI_PAGE_DEBUG);

    // まず既定の最終配置（0x12, 0x1A, 0x22, 0x2A, 0x30）と 0x0A（作業用）をスキャン
    int present[GP_NUM] = {
        I2C_probe(gp_target_addr[0]),  //
        I2C_probe(gp_target_addr[1]),  //
        I2C_probe(gp_target_addr[2]),  //
        I2C_probe(gp_target_addr[3]),  //
        I2C_probe(gp_target_addr[4]),
    };
    int clr_present = I2C_probe(gp_target_addr_cleared);
    int def_present = I2C_probe(gp_target_addr_default);

    if (!present[0] && !present[1] && !present[2] && !present[3] && !present[4] && !def_present) {
        // どこにも居ない → 表示して終了
        show_not_found_and_exit();
        return;
    }

    ui_log(UI_LOG_LEVEL_TRACE, "GP found ");
    if (present[0]) {
        ui_log(UI_LOG_LEVEL_TRACE, "1 ");
    }
    if (present[1]) {
        ui_log(UI_LOG_LEVEL_TRACE, "2 ");
    }
    if (present[2]) {
        ui_log(UI_LOG_LEVEL_TRACE, "3 ");
    }
    if (present[3]) {
        ui_log(UI_LOG_LEVEL_TRACE, "4 ");
    }
    if (present[4]) {
        ui_log(UI_LOG_LEVEL_TRACE, "5 ");
    }
    if (def_present) {
        ui_log(UI_LOG_LEVEL_TRACE, "def ");
    }
    if (clr_present) {
        ui_log(UI_LOG_LEVEL_TRACE, "clr ");
    }
    ui_log(UI_LOG_LEVEL_TRACE, "\n");
    Delay_Ms(1500);

    // すべて見えている場合でも「差分があれば上書き」する
    for (;;) {
        // 全員が最終番地に居るか？
        bool all_seen = true;
        for (int i = 0; i < GP_NUM; i++) {
            if (!present[i]) {
                all_seen = false;
                break;
            }
        }

        // 居るものは verify→差分があれば上書き（最終番地側へ書く）
        for (int i = 0; i < GP_NUM; i++) {
            if (present[i] && gp_img[i].size > 0) {
                int same = gp_compare_image(gp_target_addr[i], gp_img[i].base, gp_img[i].image, gp_img[i].size - 0x10);
                if (same) {
                    ui_log(UI_LOG_LEVEL_INFO, "firm is ok:");
                    ui_printD(UI_PAGE_LOG, i + 1);
                    ui_log(UI_LOG_LEVEL_INFO, "\n");
                } else {
                    ui_log(UI_LOG_LEVEL_INFO, "reprogramming:");
                    ui_printD(UI_PAGE_LOG, i + 1);
                    ui_log(UI_LOG_LEVEL_INFO, "\n");
                    gp_program_with_erase(gp_target_addr[i], gp_img[i].base, gp_img[i].image, gp_img[i].size);
                    ui_log(UI_LOG_LEVEL_INFO, "done");
                    ui_log(UI_LOG_LEVEL_INFO, "\n");
                }
            }
        }

        if (all_seen) {
            // 全員在席＋必要なら上書き済み → 完了
            return;
        }
        ui_log(UI_LOG_LEVEL_INFO, "not all seen\n");

        // まだ見えていない最初のICを 0x0A（作業用）経由でプログラム
        // （基板のショートジャンパで、そのICだけがバスに接続されている前提）
        int target = -1;
        for (int i = 0; i < GP_NUM; i++) {
            if (!present[i]) {
                target = i;
                break;
            }
        }

        if (target < 0) {
            // 論理上起きないはずだが保険
            ui_log(UI_LOG_LEVEL_INFO, "???\n");
            return;
        }

        if (!I2C_probe(gp_target_addr_default)) {
            // 作業用 0x0A 自体が見えない → 表示して終了
            show_not_found_and_exit();
            return;
        }

        if (gp_img[target].size > 0) {
            ui_log(UI_LOG_LEVEL_INFO, "programming GP");
            ui_printD(UI_PAGE_LOG, target + 1);
            ui_log(UI_LOG_LEVEL_INFO, "\n");
            // 0x0A 側へ書き込む（書込み完了でICが自動的に再配置される前提）
            gp_program_with_erase(gp_target_addr_default, gp_img[target].base, gp_img[target].image, gp_img[target].size);
            ui_log(UI_LOG_LEVEL_INFO, "done");
            ui_log(UI_LOG_LEVEL_INFO, "\n");
        } else {
            ui_log(UI_LOG_LEVEL_INFO, "data empty: Skip GP");
            ui_printD(UI_PAGE_LOG, target + 1);
            ui_log(UI_LOG_LEVEL_INFO, "\n");
        }

        // 再スキャン（当該ICが最終番地に現れるはず）
        present[target] = I2C_probe(gp_target_addr[target]);
        // 次ループへ
    }
}
