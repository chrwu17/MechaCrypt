// website.h
#ifndef WEBSITE_H
#define WEBSITE_H

#include <stdint.h>
#include <stddef.h>
#include <stm32l432xx.h>
#include "../lib/STM32L432KC_USART.h"

#ifdef __cplusplus
extern "C" {
#endif

// Serve one HTTP request (blocks until a full request is received)
void processWebRequest(USART_TypeDef *USART, uint8_t *precision, int *led_status);

// ---------------- Storage / API for FPGA handoff ----------------
#define WEBSITE_BLOCK_SIZE 16

// How many 16-byte blocks are currently stored (0..max)
size_t website_total_blocks(void);

// Copy block #i (0-based) into out[16]. Returns 1 on success, 0 if out of range.
int website_get_block(size_t i, uint8_t out[WEBSITE_BLOCK_SIZE]);

// Pop (consume) the oldest block into out[16]. Returns 1 if popped, 0 if none.
int website_pop_block(uint8_t out[WEBSITE_BLOCK_SIZE]);

// Clear all stored blocks
void website_clear_blocks(void);

// Returns 1 if the last request populated blocks (i.e., new data since last clear/pop loop).
int website_has_new_data(void);

#ifdef __cplusplus
}
#endif

#endif // WEBSITE_H
