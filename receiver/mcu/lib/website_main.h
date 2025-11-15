// ========================================
// FILE: receiver/mcu/lib/main.h
// ========================================
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "STM32L432KC.h"
#include "STM32L432KC_GPIO.h"
#include "STM32L432KC_USART.h"
#include "STM32L432KC_TIM.h"

// ===== Project-wide constants =====
#define LED_PIN        PB3          // Adjust to your board's LED if different
#define READY_PIN      PA5          // MCU -> FPGA (ready to receive)
#define VALID_PIN      PA6          // FPGA -> MCU (data valid signal)

#define MAX_BLOCKS     64           // number of blocks that can be received
#define BUFF_LEN       512          // HTTP request line buffer

// ===== System bring-up (provided elsewhere) =====
void configureFlash(void);
void configureClock(void);

// ===== Web handling =====
void processWebRequest(USART_TypeDef *USART);

// ===== SPI receive functions =====
void init_spi_receiver(void);          // Initialize SPI and handshake pins
void poll_spi_receive(void);           // Poll for incoming data from FPGA
void store_received_block(int block_idx, const uint8_t *block16);  // Store received block

#endif // MAIN_H