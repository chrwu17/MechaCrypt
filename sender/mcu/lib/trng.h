#ifndef TRNG_H
#define TRNG_H

#include <stdint.h>

// Function to initialize the TRNG (no arguments, no return value)
void initTRNG(void);

// Function to read 128 bits of random data into a provided buffer
void read_trng(uint8_t *buffer);

#endif // TRNG_H
