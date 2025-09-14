// greenpak_program.c など

#include <stdint.h>
#include "i2c/i2c_ch32x035.h"

#define GP_REG_ER_SR 0xE3                      // Erase Status/Control Register (ERSR)
#define GP_ERSE(page) (0x80 | ((page) & 0x0F)) // bit7=ERSE, 下位4bit=ページ
#define GP_PAGE_SIZE 16
#define GP_ERASE_T_MS 500 // ts版に合わせて 1s（安全側）。短縮可。:contentReference[oaicite:3]{index=3}

static void gp_erase_page(uint8_t reg_addr7, uint8_t page)
{
    // レジスタ空間は「NVMアドレス−1」。8bitアドレスで渡す仕様なので <<1|0
    I2C_start((reg_addr7 << 1) | 0); // SLA+W  :contentReference[oaicite:4]{index=4}
    I2C_write(GP_REG_ER_SR);         // ERSR = 0xE3
    I2C_write(GP_ERSE(page));        // 0x80 | page
    I2C_stop();                      // STOP   :contentReference[oaicite:5]{index=5}
    Delay_Ms(GP_ERASE_T_MS);         // 自己タイミング消去の完了待ち
}

static void gp_erase_range(uint8_t nvm_addr7, uint16_t base, uint16_t size)
{
    uint8_t reg_addr7 = (uint8_t)(nvm_addr7 - 1); // ts版と同じ「NVM−1」へ出す  :contentReference[oaicite:6]{index=6}
    if (!size)
        return;
    uint16_t start_page = (base) / GP_PAGE_SIZE;
    uint16_t end_page = (base + size - 1) / GP_PAGE_SIZE;
    if (start_page > 15)
        start_page = 15;
    if (end_page > 15)
        end_page = 15;
    for (uint16_t p = start_page; p <= end_page; ++p)
    {
        gp_erase_page(reg_addr7, (uint8_t)p);
    }
}

// 既存の連続書き込みを流用（そのまま）
extern void gp_write_seq(uint8_t addr7, uint16_t start, const uint8_t *data, uint16_t len);

// 消去→書込みワンショット
void gp_program_with_erase(uint8_t nvm_addr7, uint16_t base, const uint8_t *img, uint16_t size)
{
    if (!size)
        return;
    gp_erase_range(nvm_addr7, base, size); // 先に消去
    for (int i = 0; i < 16; i++)
    {
        // 16バイトずつウエイトを入れて書く
        Delay_Ms(100);
        gp_write_seq(nvm_addr7, base + i * 16, &img[i * 16], (size - i * 16 > 16) ? 16 : (size - i * 16));
    }
}
