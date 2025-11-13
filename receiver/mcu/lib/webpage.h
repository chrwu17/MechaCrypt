
// ========================================
// FILE: receiver/mcu/lib/webpage.h
// ========================================
#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <stdint.h>
#include "STM32L432KC_USART.h"

// HTML server & request handler
void processWebRequest(USART_TypeDef *USART);

// Store a received block from FPGA (called from main.c SPI receive)
void store_received_block(int block_idx, const uint8_t *block16);

#endif // WEBPAGE_H