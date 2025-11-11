#include "../lib/trng.h"
#include <stm32l432xx.h>

// Dynamic array to hold the generated keys (user-defined number)
uint8_t keys[256][16];  // Max of 256 keys, each 128-bits (16 bytes)

// Function to initialize the TRNG
void initTRNG(void) {
    // Enable the RNG peripheral clock (RNGEN)
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    
    // Enable the RNG
    RNG->CR |= RNG_CR_RNGEN;  // Fixed: Must enable RNG before waiting
    
    // Wait until the TRNG is ready
    while (!(RNG->SR & RNG_SR_DRDY)) {
        __NOP();
    }
}

// Function to read 128 bits from the TRNG (for a key)
// Fixed: Function name matches header declaration
void read_trng(uint8_t *buffer) {
    // Read 32 bits from the RNG and fill the buffer (4 * 32 bits = 128 bits)
    uint32_t *buf32 = (uint32_t *)buffer;
    
    // Fixed: Need to wait for data ready before EACH read
    for (int i = 0; i < 4; i++) {
        while (!(RNG->SR & RNG_SR_DRDY)) {
            __NOP();
        }
        buf32[i] = RNG->DR;
    }
}

// Function to generate 'numKeys' random 128-bit keys using the TRNG
void generateKeys(uint8_t numKeys) {
    for (int i = 0; i < numKeys; i++) {
        read_trng(keys[i]);
    }
}