/**
 * @file msgReceive.h
 * @author Josaphat Ngoga (modified by Christian Wu)
 * @date  19/11/2025
 * @brief Header file for message receiving functions and variables.
 *        Receives CIPHERTEXT from mechanical system.
*/

#ifndef MSGRECEIVE_H
#define MSGRECEIVE_H

#include <stdint.h>
#include <stm32l432xx.h>
#include "STM32L432KC_GPIO.h"

///////////////////////////////////////////////////////////////////////////////
// Pin Definitions
///////////////////////////////////////////////////////////////////////////////

#define TX_CLK PA6   // Pin for transfer clock line (triggers on rising edge)
#define BIT_0 PA0    // Pin for bit 0 line
#define BIT_1 PA1    // Pin for bit 1 line 
#define BIT_2 PA2    // Pin for bit 2 line
#define BIT_3 PA3    // Pin for bit 3 line
#define BIT_4 PA4    // Pin for bit 4 line
#define BIT_5 PA5    // Pin for bit 5 line
#define BIT_6 PA7    // Pin for bit 6 line
#define BIT_7 PA12   // Pin for bit 7 line

// Number of bytes per block (128 bits = 16 bytes for AES)
#define MSG_BYTES 16

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Initialize GPIO pins and interrupt for message reception
 *        Sets up EXTI interrupt on TX_CLK pin
 */
void initMsgReceive(void);

/**
 * @brief ISR handler logic (called from EXTI interrupt)
 *        DO NOT call directly - called automatically on TX_CLK rising edge
 */
void msgReceiveISR(void);

/**
 * @brief Check if a complete ciphertext block has been received
 * @return 1 if block is ready, 0 otherwise
 */
uint8_t isBlockReady(void);

/**
 * @brief Get the current ciphertext block
 * @param buffer 16-byte buffer to store ciphertext
 * 
 * Call this after isBlockReady() returns 1
 */
void getCurrentCiphertextBlock(uint8_t buffer[MSG_BYTES]);

/**
 * @brief Clear the block ready flag
 * 
 * Call this after you've processed the block
 */
void clearBlockReadyFlag(void);

/**
 * @brief Increment the received block counter and update LCD
 * 
 * Call this after successfully decrypting a block
 */
void incrementBlockCount(void);

/**
 * @brief Get the number of blocks received so far
 * @return Number of complete 16-byte blocks received and processed
 */
uint16_t getMsgReceivedBlockCount(void);

/**
 * @brief Reset the message receiver (call when starting new transfer)
 */
void resetMsgReceive(void);

#endif