/**
 * @file main.h
 * @author Christian Wu
 * @date 2024-11-19
 * @brief Main header file for MechaCrypt Sender MCU
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "STM32L432KC.h"
#include "webpage.h"
#include "trng.h"

// ===== Project-wide constants =====
#define LED_PIN        PB8         // Debug LED
#define LOAD_PIN       PA5          // MCU -> FPGA (start/load)
#define DONE_PIN       PA6          // FPGA -> MCU (done signal)

#define MAX_BLOCKS     64           // number of blocks UI can stage
#define BUFF_LEN       512          // HTTP request line buffer

// ===== System bring-up (provided elsewhere) =====
void configureFlash(void);
void configureClock(void);

// ===== Web handling =====
void processWebRequest(USART_TypeDef *USART);

// ===== SPI / Handshake helpers (implemented in webpage.c) =====
void mechacrypt_init_io_and_spi(void);       // config LOAD/DONE pins + initSPI
void mechacrypt_poll_and_advance(void);      // poll DONE and send next ready block (if any)
void mechacrypt_maybe_start_after_block(int block_idx); // start send if this was first block (idx 0)

#endif // MAIN_H
