/* ==========================
 * File: webpage.h
 * ========================== */
#ifndef WEBPAGE_H
#define WEBPAGE_H

#include "STM32L432KC.h"
#include <stdint.h>

// ---- Tunables ----
#ifndef MAX_BLOCKS
#define MAX_BLOCKS 128   // number of 16-byte blocks kept in RAM
#endif

#ifndef BUFF_LEN
#define BUFF_LEN 512     // small HTTP request line buffer
#endif

#ifndef LED_PIN
#define LED_PIN 5        // On Nucleo L432KC: PA5 user LED
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Global RX state (owned by main.c, used by webpage.c)
extern volatile uint16_t total_blocks;   // number of blocks seen (= highest index+1 contiguous)
extern volatile uint16_t next_to_send;   // (kept for compatibility with your TX state machine)
extern volatile uint8_t  start_send;     // (same)

extern uint8_t plaintext_blocks[MAX_BLOCKS][16];
extern uint8_t have_block[MAX_BLOCKS];

// Your existing sender UI page (defined in webpage.c from your project)
extern const char webpage[];

// HTTP request handler. Call this whenever a full HTTP request is available on USART.
void processWebRequest(USART_TypeDef *USART);

#ifdef __cplusplus
}
#endif

#endif // WEBPAGE_H
