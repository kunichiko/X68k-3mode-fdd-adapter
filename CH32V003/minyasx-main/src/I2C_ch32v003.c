/*
 * Hardware I2C Master Functions for CH32V003
 * Compatible interface with software I2C (i2c_soft.h)
 * 2025 - Hardware I2C version based on ssd1306_i2c.h style
 */

#include "I2C_ch32v003.h"

/*
 * error descriptions
 */
char *i2c_errstr[] =
{
    "not busy",
    "master mode",
    "transmit mode", 
    "tx empty",
    "transmit complete",
    "receive mode",
    "byte received"
};

/*
 * error handler
 */
uint8_t I2C_error(uint8_t err)
{
    // Note: printf may not be available in all environments
    // printf("I2C_error - timeout waiting for %s\n\r", i2c_errstr[err]);
    
    // reset & initialize I2C
    I2C_init();
    return 1;
}

/*
 * check for 32-bit event codes
 */
uint8_t I2C_chk_evt(uint32_t event_mask)
{
    /* read order matters here! STAR1 before STAR2!! */
    uint32_t status = I2C1->STAR1 | (I2C1->STAR2 << 16);
    return (status & event_mask) == event_mask;
}

/*
 * wait for I2C event with timeout
 */
uint8_t I2C_wait_evt(uint32_t event, uint8_t err_code)
{
    uint32_t timeout = I2C_TIMEOUT_MAX;
    while (!I2C_chk_evt(event) && timeout--);
    if (timeout == 0) {
        return I2C_error(err_code);
    }
    return 0;
}

/*
 * init I2C hardware
 */
void I2C_init(void)
{
    uint16_t tempreg;
    
    // Enable I2C1 and GPIOC clocks
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    
    // Reset I2C1 to init all regs
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;
    
    // Configure GPIO pins for I2C
    // PC1 (SDA) as alternate function open-drain, 10MHz
    GPIOC->CFGLR &= ~(0xF << (4 * 1));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 1);
    
    // PC2 (SCL) as alternate function open-drain, 10MHz  
    GPIOC->CFGLR &= ~(0xF << (4 * 2));
    GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 2);
    
    // set freq
    tempreg = I2C1->CTLR2;
    tempreg &= ~I2C_CTLR2_FREQ;
    tempreg |= (I2C_PRERATE / 1000000) & I2C_CTLR2_FREQ;
    I2C1->CTLR2 = tempreg;
    
    // Set clock config
    tempreg = 0;
#if (I2C_CLKRATE <= 100000)
    // standard mode good to 100kHz
    tempreg = (I2C_PRERATE / (2 * I2C_CLKRATE)) & I2C_CKCFGR_CCR;
#else
    // fast mode over 100kHz
    tempreg = (I2C_PRERATE / (3 * I2C_CLKRATE)) & I2C_CKCFGR_CCR;
    tempreg |= I2C_CKCFGR_FS;
#endif
    if (tempreg == 0) tempreg = 1;  // Minimum value
    I2C1->CKCFGR = tempreg;
    
    // Enable I2C
    I2C1->CTLR1 |= I2C_CTLR1_PE;
    
    // set ACK mode
    I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

/*
 * I2C start transmission
 */
void I2C_start(uint8_t addr)
{
    uint32_t timeout;
    
    // wait for not busy
    timeout = I2C_TIMEOUT_MAX;
    while ((I2C1->STAR2 & I2C_STAR2_BUSY) && timeout--);
    if (timeout == 0) {
        I2C_error(0);
        return;
    }
    
    // Set START condition
    I2C1->CTLR1 |= I2C_CTLR1_START;
    
    // wait for master mode select
    if (I2C_wait_evt(I2C_EVENT_MASTER_MODE_SELECT, 1) != 0) return;
    
    // send 7-bit address
    I2C1->DATAR = addr;
    
    // wait for transmit/receive condition  
    if (addr & 0x01) {
        // Read mode
        I2C_wait_evt(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 5);
    } else {
        // Write mode
        I2C_wait_evt(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 2);
    }
}

/*
 * I2C restart transmission  
 */
void I2C_restart(uint8_t addr)
{
    // Set START condition (restart)
    I2C1->CTLR1 |= I2C_CTLR1_START;
    
    // wait for master mode select
    if (I2C_wait_evt(I2C_EVENT_MASTER_MODE_SELECT, 1) != 0) return;
    
    // send 7-bit address
    I2C1->DATAR = addr;
    
    // wait for transmit/receive condition
    if (addr & 0x01) {
        // Read mode
        I2C_wait_evt(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 5);
    } else {
        // Write mode  
        I2C_wait_evt(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 2);
    }
}

/*
 * I2C stop transmission
 */
void I2C_stop(void)
{
    // set STOP condition
    I2C1->CTLR1 |= I2C_CTLR1_STOP;
}

/*
 * I2C transmit one data byte to the slave
 */
void I2C_write(uint8_t data)
{
    uint32_t timeout;
    
    // wait for TX Empty
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->STAR1 & I2C_STAR1_TXE) && timeout--);
    if (timeout == 0) {
        I2C_error(3);
        return;
    }
    
    // send data
    I2C1->DATAR = data;
    
    // wait for tx complete
    I2C_wait_evt(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 4);
}

/*
 * I2C receive one data byte from the slave (ack=0 for last byte, ack>0 if more bytes to follow)
 */
uint8_t I2C_read(uint8_t ack)
{
    uint8_t data = 0;
    
    if (ack) {
        // Enable ACK for next byte
        I2C1->CTLR1 |= I2C_CTLR1_ACK;
    } else {
        // Disable ACK for last byte (NACK)
        I2C1->CTLR1 &= ~I2C_CTLR1_ACK;
    }
    
    // wait for byte received
    if (I2C_wait_evt(I2C_EVENT_MASTER_BYTE_RECEIVED, 6) == 0) {
        // read received data
        data = I2C1->DATAR;
    }
    
    return data;
}

/*
 * Send data buffer via I2C bus and stop
 */
void I2C_writeBuffer(uint8_t* buf, uint16_t len)
{
    while (len--) I2C_write(*buf++);
    I2C_stop();
}

/*
 * Read data via I2C bus to buffer and stop
 */  
void I2C_readBuffer(uint8_t* buf, uint16_t len)
{
    while (len--) *buf++ = I2C_read(len > 0);
    I2C_stop();
}