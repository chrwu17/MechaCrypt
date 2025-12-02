/**
 * @file webpage.h
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Header file for receiver webpage functions.
 */

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <stdint.h>
#include "STM32L432KC.h"

#define MAX_BLOCKS     64           // Maximum number of blocks that can be stored
#define BUFF_LEN       512          // HTTP request line buffer size

/**
 * Shared state for received plaintext blocks from FPGA.
 * 
 * Convention:
 *  - Blocks are 16-byte plaintext chunks (decrypted ciphertext)
 *  - total_received is the number of sequential blocks starting at 0
 *    that have been filled (i.e., valid indices: 0 .. total_received-1)
 */
extern volatile uint8_t plaintext_blocks[MAX_BLOCKS][16];
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
 *   GET /data      -> JSON dump of plaintext blocks
 *   GET /clear     -> clear all stored blocks (returns 204 No Content)
 */
void processWebRequest(USART_TypeDef *USART);

/**
 * Helper for FPGA decryption code:
 *   Record a newly decrypted 16-byte plaintext block at index idx.
 *   Updates have_received[] and total_received accordingly.
 * 
 * @param idx Block index (0-63)
 * @param plaintext 16-byte plaintext block
 */
void receiver_store_block(uint16_t idx, const uint8_t plaintext[16]);

#endif // WEBPAGE_H