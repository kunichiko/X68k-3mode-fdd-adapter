#include "preferences/ch32_flash.h"

#include <stdint.h>

#include "ch32fun.h"
#include "ui/ui_control.h"

#define FLASH_PAGE_SIZE CH32_FLASH_PAGE_SIZE  // CH32X035は256バイト単位
#define FLASH_BASE_ADDR (0x08000000)          // Code Flash 0x08000000 - 0x0800F7FF (62KB)
#define FLASH_PAGE_N (248)                    // Code Flashのページ数 (62KB/256B=248ページ)
#define FLASH_LAST_PAGE (FLASH_PAGE_N - 1)    // 最後のページを設定保存用に使う (0始まりなので-1)
#define FLASH_LAST_ADDR (FLASH_BASE_ADDR + FLASH_PAGE_SIZE * FLASH_LAST_PAGE)

#define CRC16_POLY 0x1021  // CRC-16-CCITTの多項式

static uint8_t buffer[FLASH_PAGE_SIZE];

#if 1
#include "ui/ui_control.h"
#define debugprint(...) ui_logf(UI_LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define debugprint(...)
#endif

/**
 * @brief CRC16の計算
 * @param crc 現在のCRC値
 * @param data 追加するバイトデータ
 * @return 更新されたCRC値
 */
uint16_t crc16_update(uint16_t crc, uint8_t data) {
    crc ^= (uint16_t)(data << 8);
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ CRC16_POLY;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief フラッシュメモリからデータを読み出す
 * エラーがあった場合、バッファはゼロクリアし、 falseを返す
 */
bool ch32flash_init(void) {
    uint8_t *ptr = (uint8_t *)FLASH_LAST_ADDR;
    for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
        buffer[i] = ptr[i];
    }

    // CRCの検証
    uint16_t crc = 0;
    for (int i = 0; i < CH32_FLASH_DATA_SIZE; i++) {
        crc = crc16_update(crc, buffer[i]);
    }

    uint16_t crc_read = *(uint16_t *)&buffer[CH32_FLASH_DATA_SIZE];
    if (crc != crc_read) {
        debugprint("CRC Error\n");
        // CRCエラー → データを初期化
        for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
            buffer[i] = 0x00;
        }
        return false;
    }
    return true;
}

/**
 * @brief フラッシュメモリの指定アドレスのバイトデータを取得
 */
uint8_t ch32flash_get_byte(uint32_t addr) {
    if (addr >= CH32_FLASH_PAGE_SIZE) {
        return 0xff;
    }
    return buffer[addr];
}

/**
 * @brief バッファにバイトデータを書き込む（フラッシュには未反映）
 */
void ch32flash_set_byte(uint32_t addr, uint8_t data) {
    if (addr >= CH32_FLASH_DATA_SIZE) {  // CRC領域を変更しないようにする
        return;
    }
    buffer[addr] = data;
}

/**
 * @brief フラッシュメモリの指定アドレスのワードデータを取得
 */
uint32_t ch32flash_get_word(uint32_t addr) {
    if (addr > CH32_FLASH_DATA_SIZE - 4) {  // CRC領域を変更しないようにする (wordなので-4)
        return 0xffffffff;
    }
    return *(uint32_t *)&buffer[addr];
}

/**
 * @brief バッファにワードデータを書き込む（フラッシュには未反映）
 */
void ch32flash_set_word(uint32_t addr, uint32_t data) {
    if (addr > CH32_FLASH_DATA_SIZE - 4) {  // CRC領域を変更しないようにする (wordなので-4)
        return;
    }
    *(uint32_t *)&buffer[addr] = data;
}

/**
 * @brief バッファの内容をクリアします（0x00で埋める）
 */
void ch32flash_clear_data(void) {
    for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
        buffer[i] = 0x00;
    }
}

/**
 * @brief フラッシュメモリのページを消去
 */
static void flash_erase(void) {
    if (FLASH->CTLR & FLASH_CTLR_LOCK) {
        // ロックされている場合は解除する
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }
    if (FLASH->CTLR & FLASH_CTLR_FLOCK) {
        // Fast programming lockされている場合は解除する
        FLASH->MODEKEYR = 0x45670123;
        FLASH->MODEKEYR = 0xCDEF89AB;
    }
    // 消去プロセス
    FLASH->CTLR = FLASH_CTLR_FTER;
    FLASH->ADDR = (intptr_t)FLASH_LAST_ADDR;
    FLASH->CTLR = FLASH_CTLR_STRT | FLASH_CTLR_FTER;

    // 消去完了を待つ
    while (FLASH->STATR & FLASH_STATR_BSY);
}

/**
 * @brief バッファのデータをフラッシュメモリに書き込む
 */
void ch32flash_commit(void) {
    // CRCを計算して書き込む
    uint16_t crc = 0;
    for (int i = 0; i < CH32_FLASH_DATA_SIZE; i++) {
        crc = crc16_update(crc, buffer[i]);
    }
    *(uint16_t *)&buffer[CH32_FLASH_DATA_SIZE] = crc;  // 最後の2バイトにCRCを書き込む

    flash_erase();  // まずページを消去する

    // 書き込み準備
    // PG: Perform fast page programming operation
    // BUF_RST: Reset the programming data buffer
    FLASH->CTLR = FLASH_CTLR_FTPG | FLASH_CTLR_BUFRST;
    // 準備完了を待つ
    while (FLASH->STATR & FLASH_STATR_BSY);
    FLASH->STATR |= FLASH_STATR_EOP;  // EOPフラグをクリア(1をセットするとクリアされる)

    // バッファのデータをフラッシュの書き込みバッファにロード
    // 32bit単位で書き込む必要がある
    uint32_t *flash_ptr = (uint32_t *)FLASH_LAST_ADDR;
    // ================= 理由がわからないが、最初にダミーの書き込みがないと先頭4バイトが 0xffffffff になってしまう =================
    *flash_ptr = 0;
    FLASH->CTLR = FLASH_CTLR_FTPG | FLASH_CTLR_BUFLOAD;
    while (FLASH->STATR & FLASH_STATR_BSY);
    // ================= ここまで =================
    for (int i = 0; i < CH32_FLASH_PAGE_SIZE; i += 4) {
        uint32_t val = *(uint32_t *)&buffer[i];
        *(flash_ptr + i / 4) = val;
        FLASH->CTLR = FLASH_CTLR_FTPG | FLASH_CTLR_BUFLOAD;
        while (FLASH->STATR & FLASH_STATR_BSY);
    }

    // 書き込みアドレスをセット
    FLASH->ADDR = (intptr_t)FLASH_LAST_ADDR;

    // フラッシュへの書き込み開始
    FLASH->CTLR = FLASH_CTLR_FTPG | FLASH_CTLR_STRT;

    // 書き込み完了を待つ
    while (FLASH->STATR & FLASH_STATR_BSY);

    debugprint("Flash Commit Status:0x%02x\n", FLASH->STATR);
}

void ch32flash_dumpflash() {
    debugprint("Dump Flash\n");

    uint8_t *ptr = (uint8_t *)0x08000000;
    for (int page = FLASH_LAST_PAGE; page < FLASH_PAGE_N; page++) {  // Code Flash 62KBの最後の256バイトだけ表示
        debugprint("Page: %d\n", page);
        ptr = (uint8_t *)(0x08000000 + page * FLASH_PAGE_SIZE);
        for (int i = 0; i < FLASH_PAGE_SIZE / 4; i++) {
            debugprint("%08x: %02x%02x%02x%02x\n", (uint32_t)ptr, ptr[0], ptr[1], ptr[2], ptr[3]);
            ptr += 4;
        }
    }
}

void ch32flash_dumpmemory() {
    debugprint("Dump Memory\n");

    for (int i = 0; i < FLASH_PAGE_SIZE; i += 8) {
        debugprint("%02x%02x%02x%02x-%02x%02x%02x%02x\n", buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3], buffer[i + 4],
                   buffer[i + 5], buffer[i + 6], buffer[i + 7]);
    }
}