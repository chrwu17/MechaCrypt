/**
 * @file msgReceive.h
 * @author Josaphat Ngoga
 * @date  19/11/2025
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

<<<<<<< HEAD
#define TX_CLK PA0   // Pin for transfer clock line (triggers on rising edge)
#define BIT_0 PA1    // Pin for bit 0 line
#define BIT_1 PA2    // Pin for bit 1 line 
#define BIT_2 PA3    // Pin for bit 2 line
#define BIT_3 PA4    // Pin for bit 3 line
#define BIT_4 PA5    // Pin for bit 4 line
#define BIT_5 PA6    // Pin for bit 5 line
=======
#define TX_CLK PA6   // Pin for transfer clock line
#define BIT_0 PA0    // Pin for bit 0 line
#define BIT_1 PA1    // Pin for bit 1 line 
#define BIT_2 PA2    // Pin for bit 2 line
#define BIT_3 PA3    // Pin for bit 3 line
#define BIT_4 PA4    // Pin for bit 4 line
#define BIT_5 PA5    // Pin for bit 5 line
>>>>>>> parent of 40e5393 (testing receiver end fixes)
#define BIT_6 PA7    // Pin for bit 6 line
#define BIT_7 PA12   // Pin for bit 7 line

// Number of bytes to receive
#define MSG_BYTES 16

///////////////////////////////////////////////////////////////////////////////
// External Variables
///////////////////////////////////////////////////////////////////////////////

// Buffer to store received message
extern volatile uint8_t receivedMessage[MSG_BYTES];

// Flag to indicate message reception completion
extern volatile uint8_t messageReceivedFlag;

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

// Configure GPIO pins + interrupt for TX_CLK
void initMsgReceive(void);

// ISR handler logic
void msgReceiveISR(void);

#endif 