// STM32L432KC_I2C_bb.c
// Bit-banged I2C (Standard-mode-ish) using CMSIS on PB8/PB9

#include "../lib/STM32L432KC_I2C_bb.h"

static inline void pin_to_input(GPIO_TypeDef *g, uint32_t pin) {
    g->MODER &= ~(0x3u << (pin * 2));        // 00: input
}

static inline void pin_to_output_od(GPIO_TypeDef *g, uint32_t pin) {
    g->MODER = (g->MODER & ~(0x3u << (pin * 2))) | (0x1u << (pin * 2)); // 01: output
    g->OTYPER |= (1u << pin);               // open-drain
    g->OSPEEDR |= (0x3u << (pin * 2));      // high speed (cleaner edges)
}

static inline void drive_low(GPIO_TypeDef *g, uint32_t pin) {
    pin_to_output_od(g, pin);
    g->ODR &= ~(1u << pin);                 // drive 0
}

static inline void release_line(GPIO_TypeDef *g, uint32_t pin) {
    pin_to_input(g, pin);                   // let external pull-up pull it high
}

static inline uint32_t read_line(GPIO_TypeDef *g, uint32_t pin) {
    return (g->IDR >> pin) & 1u;
}

// small delay to shape SCL
static inline void i2c_delay(void) {
    for (volatile int i = 0; i < 80; ++i) __NOP();
}

static void i2c_start(void) {
    // SDA falls while SCL high
    release_line(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    release_line(I2C_BB_GPIO, I2C_BB_SCL_PIN);
    i2c_delay();
    drive_low(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    i2c_delay();
    drive_low(I2C_BB_GPIO, I2C_BB_SCL_PIN);
    i2c_delay();
}

static void i2c_stop(void) {
    // SDA rises while SCL high
    drive_low(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    i2c_delay();
    release_line(I2C_BB_GPIO, I2C_BB_SCL_PIN);
    i2c_delay();
    release_line(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    i2c_delay();
}

static void i2c_write_bit(uint8_t b) {
    if (b) release_line(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    else   drive_low(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    i2c_delay();
    release_line(I2C_BB_GPIO, I2C_BB_SCL_PIN);  // SCL high
    i2c_delay();
    drive_low(I2C_BB_GPIO, I2C_BB_SCL_PIN);     // SCL low
    i2c_delay();
}

static uint8_t i2c_read_bit(void) {
    release_line(I2C_BB_GPIO, I2C_BB_SDA_PIN);  // float to read
    i2c_delay();
    release_line(I2C_BB_GPIO, I2C_BB_SCL_PIN);
    i2c_delay();
    uint8_t bit = (uint8_t)read_line(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    drive_low(I2C_BB_GPIO, I2C_BB_SCL_PIN);
    i2c_delay();
    return bit;
}

// returns 0 on ACK, 1 on NACK
static uint8_t i2c_write_byte_raw(uint8_t byte) {
    for (int i = 7; i >= 0; --i) i2c_write_bit((byte >> i) & 1u);
    return i2c_read_bit(); // ACK is 0
}

void i2c_bb_init(void) {
    I2C_BB_GPIO_EN();

    // release lines (make them inputs) so external pull-ups hold them high
    release_line(I2C_BB_GPIO, I2C_BB_SCL_PIN);
    release_line(I2C_BB_GPIO, I2C_BB_SDA_PIN);
    // settle
    for (volatile int d=0; d<2000; ++d) __NOP();
}

uint8_t i2c_bb_write(uint8_t addr7, const uint8_t *data, uint16_t len) {
    i2c_start();
    if (i2c_write_byte_raw((addr7 << 1) | 0u)) { // write direction
        i2c_stop();
        return 1; // NACK on address
    }
    for (uint16_t i = 0; i < len; ++i) {
        if (i2c_write_byte_raw(data[i])) {
            i2c_stop();
            return 1; // NACK on data
        }
    }
    i2c_stop();
    return 0; // success
}

uint8_t i2c_bb_write_byte(uint8_t addr7, uint8_t byte) {
    return i2c_bb_write(addr7, &byte, 1);
}
