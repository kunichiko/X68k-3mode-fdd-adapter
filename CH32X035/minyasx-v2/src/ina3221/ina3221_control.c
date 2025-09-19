#include "ina3221_control.h"

#include "i2c/i2c_ch32x035.h"
#include "oled/ssd1306_txt.h"

void ina3221_init(void) {
    // INA3221の初期化コードをここに追加
}

#define INA3221_ADDR 0x40  // INA3221のI2Cアドレス

uint16_t read_word_smbus(uint8_t dev7, uint8_t reg) {
    uint16_t val;
    uint8_t lsb, msb;

    // 1) デバイス(書き込み)をオープンし、コマンド(レジスタ)を書き込む
    I2C_start((dev7 << 1) | 0);  // write
    I2C_write(reg);

    // 2) リスタートして読み出しに切り替え
    I2C_restart((dev7 << 1) | 1);  // read

    // 3) 2バイト読む（SMBusは LSB, MSB の順で送られる）
    msb = I2C_read(1);  // 1バイト目: MSB → ACK（まだ続く）
    lsb = I2C_read(0);  // 2バイト目: LSB → NACK（ここで終わり）
    I2C_stop();

    // 4) 16bit に合成（SMBus “word” は lsb が下位）
    val = ((uint16_t)msb << 8) | lsb;
    return val;
}

uint16_t conv_current(uint16_t raw) {
    // 電流のレジスタ値は50uA単位
    // 下位3bitは無視する必要がある
    // 最上位ビットが符号ビットで、負の値になることがあるが、その場合は0とする
    if (raw & 0x8000) {
        return 0;
    }
    return (raw & 0xFFF8) * 50 / 1000;  // mA単位に変換
}

uint16_t conv_voltage(uint16_t raw) {
    // 電圧のレジスタ値は1mV単位
    // 下位3bitは無視する必要がある
    // 最上位ビットが符号ビットで、負の値になることがあるが、その場合は0とする
    if (raw & 0x8000) {
        return 0;
    }
    return (raw & 0xFFF8) * 1;  // mV単位に変換
}

void ina3221_read_all_channels(uint16_t *ch1_current, uint16_t *ch1_voltage,  //
                               uint16_t *ch2_current, uint16_t *ch2_voltage,  //
                               uint16_t *ch3_current, uint16_t *ch3_voltage) {
    uint16_t reg[6];

    for (int i = 0; i < 6; i++) {
        reg[i] = read_word_smbus(INA3221_ADDR, i + 1);
    }

    if (ch1_current) *ch1_current = conv_current(reg[0]);
    if (ch1_voltage) *ch1_voltage = conv_voltage(reg[1]);
    if (ch2_current) *ch2_current = conv_current(reg[2]);
    if (ch2_voltage) *ch2_voltage = conv_voltage(reg[3]);
    if (ch3_current) *ch3_current = conv_current(reg[4]);
    if (ch3_voltage) *ch3_voltage = conv_voltage(reg[5]);
}

void ina3221_poll(uint64_t systick_ms) {
    static uint64_t last_tick = 0;
    if (systick_ms - last_tick < 1000) {
        return;
    }
    last_tick = systick_ms;

    uint16_t ch1_current, ch1_voltage, ch2_current, ch2_voltage, ch3_current, ch3_voltage;
    ina3221_read_all_channels(&ch1_current, &ch1_voltage, &ch2_current, &ch2_voltage, &ch3_current, &ch3_voltage);

    OLED_cursor(0, 0);
    OLED_printf("VBUS:%2d.%02dV %4dmA", ch1_voltage / 1000, (ch1_voltage % 1000) / 10, ch1_current);
    OLED_write('\n');
    OLED_printf("+12V:%2d.%02dV %4dmA", ch2_voltage / 1000, (ch2_voltage % 1000) / 10, ch2_current);
    OLED_write('\n');
    OLED_printf("+5V :%2d.%02dV %4dmA", ch3_voltage / 1000, (ch3_voltage % 1000) / 10, ch3_current);
    OLED_write('\n');
}