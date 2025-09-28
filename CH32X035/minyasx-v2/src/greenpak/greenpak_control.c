#include "greenpak_control.h"

#include <stdint.h>

#include "ui/ui_control.h"

// GreenPAKのI2Cアドレス（7bit）
// クリア済み、作業用、最終配置4個
const uint8_t gp_target_addr_cleared = 0x01;                            // クリア済み
const uint8_t gp_target_addr_default = 0x08;                            // 作業用
const uint8_t gp_target_addr[GP_NUM] = {0x10, 0x18, 0x20, 0x28, 0x30};  // 最終配置
static uint8_t gp_virtual_input_cache[GP_NUM] = {0, 0, 0, 0, 0};        // 仮想入力キャッシュ

uint8_t gp_reg_get(uint8_t addr7, uint8_t reg) {
    uint8_t reg_addr7 = (uint8_t)((addr7 & 0xfc) + 1);  // 0x00を使わないために+1する
    uint8_t val;
    I2C_start((reg_addr7 << 1) | 0);    // SLA+W  :contentReference[oaicite:3]{index=3}
    I2C_write(reg);                     // レジスタ番号
    I2C_restart((reg_addr7 << 1) | 1);  // SLA+R
    val = I2C_read(0);                  // NACKで1バイト読む
    I2C_stop();                         // STOP   :contentReference[oaicite:7]{index=7}
    return val;
}

void gp_reg_set(uint8_t addr7, uint8_t reg, uint8_t val) {
    uint8_t reg_addr7 = (uint8_t)((addr7 & 0xfc) + 1);  // 0x00を使わないために+1する
    I2C_start((reg_addr7 << 1) | 0);                    // SLA+W  :contentReference[oaicite:1]{index=1}
    I2C_write(reg);                                     // レジスタ番号
    I2C_write(val);                                     // 書き込みデータ
    I2C_stop();                                         // STOP   :contentReference[oaicite:2]{index=2}
}

uint8_t gp_addr_w(uint8_t addr7) {
    return (addr7 << 1) | 0;
}

uint8_t gp_addr_r(uint8_t addr7) {
    return (addr7 << 1) | 1;
}

// nibble→HEX
static inline char hex1(uint8_t v) {
    v &= 0xF;
    return v < 10 ? ('0' + v) : ('A' + v - 10);
}

// 1行分(8バイト)を "AAXX:AABBCCDDEEFFGGHH" 形式(最大19文字+NUL)に整形
static void make_line(char *dst, uint8_t addr, uint8_t off, const uint8_t *p8) {
    int k = 0;
    dst[k++] = hex1(addr >> 4);
    dst[k++] = hex1(addr & 0xF);
    dst[k++] = hex1(off >> 4);
    dst[k++] = hex1(off & 0xF);
    dst[k++] = ':';
    for (int i = 0; i < 8; i++) {
        dst[k++] = hex1(p8[i] >> 4);
        dst[k++] = hex1(p8[i] & 0xF);
    }
    dst[k] = '\0';
}

// OLEDに1ページ（8行=64B）描画
static void oled_dump_page(const uint8_t *buf, uint8_t addr, uint8_t base_off /*0 or 0x40*/) {
    uint8_t nvm_addr7 = (addr & 0xfc) | 0x02;  // NVMアドレスに変換 (addrは0x08,0x10,0x18,0x20,0x28のいずれか)

    ui_cursor(UI_PAGE_DEBUG, 0, 0);  // (x=0,y=0)から書き始める  :contentReference[oaicite:4]{index=4}
    char line[22];                   // 21文字 + 終端
    for (int row = 0; row < 8; row++) {
        uint8_t off = base_off + (row * 8);
        make_line(line, nvm_addr7, off, &buf[off]);
        ui_print(UI_PAGE_DEBUG, line);  // 1行出力  :contentReference[oaicite:5]{index=5}
        ui_write(UI_PAGE_DEBUG, '\n');  // 改行       :contentReference[oaicite:6]{index=6}
    }
}

void greenpak_dump_oled_with_addr(uint8_t addr);

// 先頭256バイトを読み出してOLEDに自動ページング表示
void greenpak_dump_oled(void) {
    ui_clear(UI_PAGE_DEBUG);  // 画面クリア  :contentReference[oaicite:9]{index=9}

    if (I2C_probe(gp_target_addr_cleared)) {
        greenpak_dump_oled_with_addr(gp_target_addr_cleared);  // 作業用
    }
    if (I2C_probe(gp_target_addr_default)) {
        greenpak_dump_oled_with_addr(gp_target_addr_default);  // 作業用
    }
    for (int i = 0; i < 4; i++) {
        uint8_t addr = gp_target_addr[i];
        if (!I2C_probe(addr)) {
            continue;  // 居なければスキップ
        }
        greenpak_dump_oled_with_addr(addr);
    }
}

void greenpak_dump_oled_with_addr(uint8_t addr) {
    uint8_t buf[256];

    uint8_t nvm_addr7 = (addr & 0xfc) | 0x02;  // NVMアドレスに変換 (addrは0x08,0x10,0x18,0x20,0x28のいずれか)

    // --- GreenPAK(0x0A) 先頭から256Bを読み出し ---
    I2C_start(gp_addr_w(nvm_addr7));    // 書き込み  :contentReference[oaicite:10]{index=10}
    I2C_write(0x00);                    // 内部アドレス=0x00  :contentReference[oaicite:11]{index=11}
    I2C_restart(gp_addr_r(nvm_addr7));  // リスタートで読み出しに切替  :contentReference[oaicite:12]{index=12}
    I2C_readBuffer(buf, 256);           // 読み終わると I2C_stop() が呼ばれる  :contentReference[oaicite:13]{index=13}

    // --- 4ページを交互に表示（64Bずつ） ---
    ui_clear(UI_PAGE_DEBUG);
    oled_dump_page(buf, nvm_addr7, 0x00);  // 0x00..0x3F
    Delay_Ms(1000);

    ui_clear(UI_PAGE_DEBUG);
    oled_dump_page(buf, nvm_addr7, 0x40);  // 0x40..0x7F
    Delay_Ms(1000);

    ui_clear(UI_PAGE_DEBUG);
    oled_dump_page(buf, nvm_addr7, 0x80);  // 0x80..0xBF
    Delay_Ms(1000);

    ui_clear(UI_PAGE_DEBUG);
    oled_dump_page(buf, nvm_addr7, 0xC0);  // 0xC0..0xFF
    Delay_Ms(1000);
}

uint8_t greenpak_get_virtualinput(int unit) {
    if (unit < 0 || unit >= GP_NUM) return 0;  // 範囲外
    return gp_virtual_input_cache[unit];
}

/**
 GreenPAKのVirtual Input (レジスタ0x7aにある) に値をセットします
 GP1-4全てに共通するのは以下のビットです
  - DriverSelect0 = VirtualInput[0] = bit7
  - DriverSelect1 = VirtualInput[1] = bit6
  VirtualInput番号とビットの並び順が逆なので注意!!!
 */
void greenpak_set_virtualinput(int unit, uint8_t val) {
    if (unit < 0 || unit >= GP_NUM) return;  // 範囲外

    gp_virtual_input_cache[unit] = val;
    gp_reg_set(gp_target_addr[unit], 0x7a, val);
}

bool greenpak_get_matrixinput(int unit, uint8_t inputno) {
    if (unit < 0 || unit >= GP_NUM) return false;   // 範囲外
    if (inputno < 0 || inputno > 63) return false;  // 範囲外

    // Matrix Input レジスタは 0x74..0x7B にあり、8個ずつ8バイトに分かれている
    uint8_t reg = 0x74 + inputno / 8;
    uint8_t val = gp_reg_get(gp_target_addr[unit], reg);
    return (val >> (inputno & 0x07)) & 0x01;
}
