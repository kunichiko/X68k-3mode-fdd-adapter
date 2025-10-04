#ifndef __CH32_FLASH_H__
#define __CH32_FLASH_H__

#include <stdbool.h>
#include <stdint.h>

// フラッシュのページサイズ (バイト単位、CH32X035は256バイト固定)
#define CH32_FLASH_PAGE_SIZE 256
// CRCのサイズ (バイト単位、2バイト固定)
#define CH32_FLASH_CRC_SIZE 2
#define CH32_FLASH_DATA_SIZE (CH32_FLASH_PAGE_SIZE - CH32_FLASH_CRC_SIZE)  // データ領域のサイズ (バイト単位、ページサイズ - CRCサイズ)

bool ch32flash_init(void);

/**
 * @brief 指定されたアドレス(0-63)から1バイトを読み出す
 *
 * @param addr
 */
uint8_t ch32flash_get_byte(uint32_t addr);

/**
 * @brief 指定されたアドレス(0-63)に1バイトを書き込む
 *
 * @param addr
 * @param data
 */
void ch32flash_set_byte(uint32_t addr, uint8_t data);

/**
 * @brief 指定されたアドレス(0-63)から4バイトを読み出す
 *
 * @param addr
 */
uint32_t ch32flash_get_word(uint32_t addr);

/**
 * @brief 指定されたアドレス(0-63)に4バイトを書き込む
 *
 * @param addr
 * @param data
 */
void ch32flash_set_word(uint32_t addr, uint32_t data);

/**
 * @brief バッファの内容をクリアします（0x00で埋める）
 */
void ch32flash_clear_data(void);

/**
 *  フラッシュに書き込みます
 */
void ch32flash_commit(void);

void ch32flash_dumpflash(void);

void ch32flash_dumpmemory(void);

#endif  // __CH32_FLASH_H__