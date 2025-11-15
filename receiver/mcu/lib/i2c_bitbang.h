#ifndef I2C_BITBANG_H
#define I2C_BITBANG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int i2c_bitbang_write(uint8_t addr7, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif
