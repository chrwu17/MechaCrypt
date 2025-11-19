/**
 * @file trng.h
 * @author Christian Wu
 * @date 2024-11-19
 * @brief Header file for True Random Number Generator (TRNG) functions.
 */

#ifndef TRNG_H
#define TRNG_H

#include <stdint.h>

// Function to initialize the TRNG (returns 0 on success, non-zero on error)
int initTRNG(void);

// Function to read 128 bits of random data into a provided buffer
// Returns 0 on success, non-zero on error
int read_trng(uint8_t *buffer);

#endif // TRNG_H