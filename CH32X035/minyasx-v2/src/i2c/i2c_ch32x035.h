/*
 * Hardware I2C Master Functions for CH32X035
 * Compatible interface with software I2C (i2c_soft.h)
 * 2025 - Hardware I2C version based on ssd1306_i2c.h style
 */

#ifndef _I2C_CH32X035_H
#define _I2C_CH32X035_H

#include "ch32fun.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C parameters - CH32X035 uses I2C1 on PC14(SDA)/PC13(SCL)
#ifndef PIN_SDA
#define PIN_SDA       PC14         // pin connected to serial data of the I2C bus
#define PIN_SCL       PC13         // pin connected to serial clock of the I2C bus
#endif

// I2C Bus clock rate - must be lower than Logic clock rate
#ifndef I2C_CLKRATE
#define I2C_CLKRATE   400000      // I2C bus clock rate in Hz
#endif

// I2C Logic clock rate - must be higher than Bus clock rate  
#ifndef I2C_PRERATE
#define I2C_PRERATE   48000000    // System core clock (48MHz typical for CH32X035)
#endif

// I2C Timeout count
#ifndef I2C_TIMEOUT_MAX
#define I2C_TIMEOUT_MAX 100000
#endif

// I2C Event Flags
#define I2C_EVENT_MASTER_MODE_SELECT              ((uint32_t)0x00030001)  /* BUSY, MSL and SB flag */
#define I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED ((uint32_t)0x00070082)  /* BUSY, MSL, ADDR, TXE and TRA flags */
#define I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED   ((uint32_t)0x00030002)  /* BUSY, MSL and ADDR flags */
#define I2C_EVENT_MASTER_BYTE_TRANSMITTED         ((uint32_t)0x00070084)  /* TRA, BUSY, MSL, TXE and BTF flags */
#define I2C_EVENT_MASTER_BYTE_RECEIVED            ((uint32_t)0x00030040)  /* BUSY, MSL and RXNE flags */

// I2C Functions - compatible interface with software I2C
void I2C_init(void);              // I2C init function
void I2C_start(uint8_t addr);     // I2C start transmission
void I2C_restart(uint8_t addr);   // I2C restart transmission
void I2C_stop(void);              // I2C stop transmission
void I2C_write(uint8_t data);     // I2C transmit one data byte to the slave
uint8_t I2C_read(uint8_t ack);    // I2C receive one data byte from the slave

void I2C_writeBuffer(uint8_t* buf, uint16_t len);
void I2C_readBuffer(uint8_t* buf, uint16_t len);

// Internal helper functions
uint8_t I2C_error(uint8_t err);
uint8_t I2C_chk_evt(uint32_t event_mask);
uint8_t I2C_wait_evt(uint32_t event, uint8_t err_code);

#ifdef __cplusplus
};
#endif

#endif /* _I2C_CH32X035_H */