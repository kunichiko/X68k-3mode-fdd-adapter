#ifndef INA3221_CONTROL_H
#define INA3221_CONTROL_H

#include <stdint.h>
#include <stdlib.h>

#include "minyasx.h"

void ina3221_init(void);

void ina3221_poll(minyasx_context_t *ctx, uint64_t systick_ms);

/**
 * @brief INA3221の全チャネルの電流と電圧を読み取る
 *       ch1_current: チャネル1の電流値を格納するポインタ
 *       ch1_voltage: チャネル1の電圧値を格納するポインタ
 *       ch2_current: チャネル2の電流値を格納するポインタ
 *       ch2_voltage: チャネル2の電圧値を格納するポインタ
 *       ch3_current: チャネル3の電流値を格納するポインタ
 *       ch3_voltage: チャネル3の電圧値を格納するポインタ
 * 電圧はmV単位、電流はmA単位で返されます。
 */
void ina3221_read_all_channels(uint16_t *ch1_current, uint16_t *ch1_voltage,  //
                               uint16_t *ch2_current, uint16_t *ch2_voltage,  //
                               uint16_t *ch3_current, uint16_t *ch3_voltage);

#endif  // INA3221_CONTROL_H