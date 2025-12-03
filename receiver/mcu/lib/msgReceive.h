/**
 * @file msgReceive.h
 * @author Josaphat Ngoga / Christian Wu
 * @date  2025-11-19
 * @brief Header file for message receiving functions and variables.
*/

#ifndef MSGRECEIVE_H
#define MSGRECEIVE_H

#include <stdint.h>
#include <stm32l432xx.h>
#include "STM32L432KC_GPIO.h"

///////////////////////////////////////////////////////////////////////////////
// Pin Definitions
///////////////////////////////////////////////////////////////////////////////

#define TX_CLK PA2   // Pin for transfer clock line (triggers on rising edge)
#define BIT_0 PA7    // Pin for bit 0 line
#define BIT_1 PA6    // Pin for bit 1 line 
#define BIT_2 PA3    // Pin for bit 2 line
#define BIT_3 PA1    // Pin for bit 3 line
#define BIT_4 PA0    // Pin for bit 4 line
#define BIT_5 PB1    // Pin for bit 5 line
#define BIT_6 PC14    // Pin for bit 6 line
#define BIT_7 PC15   // Pin for bit 7 line

// Number of bytes per block
#define MSG_BYTES 16

///////////////////////////////////////////////////////////////////////////////
// External Variables
///////////////////////////////////////////////////////////////////////////////

// Buffer to store currently receiving message block
extern volatile uint8_t receivedMessage[MSG_BYTES];

// Flag to indicate block reception completion
extern volatile uint8_t messageReceivedFlag;

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Configure GPIO pins and interrupt for message reception
 * Sets up PA0 (TX_CLK) as interrupt source on rising edge
 * Configures PA1-PA7, PA12 as data input pins
 */
void initMsgReceive(void);

/**
 * @brief ISR handler logic - reads 8 data pins and assembles byte
 * Called automatically by EXTI0 interrupt on TX_CLK rising edge
 */
void msgReceiveISR(void);

/**
 * @brief Get total number of complete blocks received
 * @return Total blocks received since startup
 */
uint32_t getTotalBlocksReceived(void);

/**
 * @brief Get current byte index within the block being received
 * @return Current byte position (0-15)
 */
uint8_t getCurrentByteIndex(void);

/**
 * @brief Get total interrupt count (for debugging)
 * @return Number of times TX_CLK interrupt has fired
 */
uint32_t getInterruptCount(void);

/**
 * @brief Get debounced interrupt count (for debugging)
 * @return Number of interrupts accepted after debouncing
 */
uint32_t getDebouncedCount(void);

/**
 * @brief Get rejected interrupt count (for debugging)
 * @return Number of interrupts rejected due to debouncing
 */
uint32_t getRejectedCount(void);

/**
 * @brief Get last byte value received (for debugging)
 * @return Last byte value that was read from data pins
 */
uint8_t getLastByteValue(void);

/**
 * @brief Reset all message reception counters and flags
 * Useful for testing or restarting reception
 */
void resetMsgReceive(void);

#endif