#include "greenpak_control.h"

#include <stdint.h>

// GreenPAKのI2Cアドレス（7bit）
// クリア済み、作業用、最終配置4個
const uint8_t gp_target_addr_cleared = 0x00;  // クリア済み
const uint8_t gp_target_addr_default = 0x08;  // 作業用
const uint8_t gp_target_addr[4] = {0x10, 0x18, 0x20, 0x28};

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

    OLED_cursor(0, 0);  // (x=0,y=0)から書き始める  :contentReference[oaicite:4]{index=4}
    char line[22];      // 21文字 + 終端
    for (int row = 0; row < 8; row++) {
        uint8_t off = base_off + (row * 8);
        make_line(line, nvm_addr7, off, &buf[off]);
        OLED_print(line);  // 1行出力  :contentReference[oaicite:5]{index=5}
        OLED_write('\n');  // 改行       :contentReference[oaicite:6]{index=6}
    }
}

void greenpak_dump_oled_with_addr(uint8_t addr);

// 先頭256バイトを読み出してOLEDに自動ページング表示
void greenpak_dump_oled(void) {
    OLED_clear();  // 画面クリア  :contentReference[oaicite:9]{index=9}

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
    OLED_clear();
    oled_dump_page(buf, nvm_addr7, 0x00);  // 0x00..0x3F
    Delay_Ms(1000);

    OLED_clear();
    oled_dump_page(buf, nvm_addr7, 0x40);  // 0x40..0x7F
    Delay_Ms(1000);

    OLED_clear();
    oled_dump_page(buf, nvm_addr7, 0x80);  // 0x80..0xBF
    Delay_Ms(1000);

    OLED_clear();
    oled_dump_page(buf, nvm_addr7, 0xC0);  // 0xC0..0xFF
    Delay_Ms(1000);
}
