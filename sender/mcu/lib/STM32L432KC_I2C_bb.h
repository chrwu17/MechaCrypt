// STM32L432KC_I2C_bb.h
// Simple CMSIS-only bit-banged I2C on PB8(SCL)/PB9(SDA) for STM32L432KC

#ifndef STM32L432KC_I2C_BB_H
#define STM32L432KC_I2C_BB_H

#include <stdint.h>
#include <stm32l432xx.h>

// Default pins (match Nucleo-L432KC silk)
#define I2C_BB_GPIO           GPIOB
#define I2C_BB_GPIO_EN()      (RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN)
#define I2C_BB_SCL_PIN        8u     // PB8  - SCL
#define I2C_BB_SDA_PIN        9u     // PB9  - SDA

// Public API
void i2c_bb_init(void);                              // set up PB8/PB9 open-drain style (via mode switching)
uint8_t i2c_bb_write(uint8_t addr7, const uint8_t *data, uint16_t len);  // returns 0 on success, 1 on NACK
uint8_t i2c_bb_write_byte(uint8_t addr7, uint8_t byte);                   // convenience

#endif
