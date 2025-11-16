// webpage.h - Receiver-side HTTP + shared state
#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <stdint.h>
#include "STM32L432KC.h"
#include "main.h"   // for MAX_BLOCKS, BUFF_LEN, LED_PIN, etc.

/**
 * Shared state for received plaintext blocks from FPGA.
 * 
 * Convention:
 *  - Blocks are 16-byte plaintext chunks.
 *  - total_received is the number of *sequential* blocks starting at 0
 *    that have been filled (i.e., valid indices: 0 .. total_received-1).
 */
extern volatile uint8_t received_blocks[MAX_BLOCKS][16];
extern volatile uint8_t have_received[MAX_BLOCKS];
extern volatile uint16_t total_received;
extern volatile uint16_t debug_request_count;

/**
 * Static HTML page served at "/".
 */
extern const char webpage[];

/**
 * Handle a single HTTP request on the given USART.
 * 
 * Endpoints:
 *   GET /          -> send HTML UI
 *   GET /data      -> JSON dump of blocks
 *   GET /clear     -> clear all stored blocks (returns 204 No Content)
 */
void processWebRequest(USART_TypeDef *USART);

/**
 * Helper for SPI / FPGA code:
 *   Record a newly received 16-byte block at index idx.
 *   Updates have_received[] and total_received accordingly.
 */
void receiver_store_block(uint16_t idx, const uint8_t blk[16]);

#endif // WEBPAGE_H
