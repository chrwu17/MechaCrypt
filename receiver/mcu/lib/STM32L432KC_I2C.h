// STM32L432KC_I2C.h
#ifndef STM32L4_I2C_H
#define STM32L4_I2C_H

#include <stdint.h>

// Initialize I2C1 on PA9/PA10 for 100kHz operation
void initI2C1(void);

// Write single byte to I2C device
// Returns 0 on success, 1 on error
int i2c_write_byte(uint8_t addr7, uint8_t data);

// Write multiple bytes to I2C device
// Returns 0 on success, negative on error
int i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len);

#endif