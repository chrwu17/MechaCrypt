/**
 * @file msgReceive.c
 * @author Josaphat Ngoga (modified by Christian Wu)
 * @date  19/11/2025
 * @brief Reads bit transfer lines from mechanical switches and records full bytes.
 *        Receives CIPHERTEXT blocks, sends to FPGA for decryption, stores plaintext.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../lib/STM32L432KC.h"
#include "../lib/msgReceive.h"
#include "../lib/webpage.h"
#include "../lib/lcd_progress.h"

// External LCD handle (defined in main.c)
extern lcd_i2c_t g_lcd;

// Current ciphertext block being received from mechanical system
volatile uint8_t currentCiphertextBlock[MSG_BYTES];
volatile uint8_t msgIndex = 0;           // Byte counter within current block
volatile uint8_t blockReadyFlag = 0;     // Flag: block ready for decryption

// Block counter for multi-block transfers
static volatile uint16_t blocks_received_count = 0;

// Initialize the receiver GPIO pins and interrupt
void initMsgReceive(void) {
    // Configure GPIO inputs for 8-bit parallel data
    pinMode(BIT_0, GPIO_INPUT);
    pinMode(BIT_1, GPIO_INPUT);
    pinMode(BIT_2, GPIO_INPUT);
    pinMode(BIT_3, GPIO_INPUT);
    pinMode(BIT_4, GPIO_INPUT);
    pinMode(BIT_5, GPIO_INPUT);
    pinMode(BIT_6, GPIO_INPUT);
    pinMode(BIT_7, GPIO_INPUT);

    // Configure TX_CLK as input
    pinMode(TX_CLK, GPIO_INPUT);

    // Initialize state
    msgIndex = 0;
    blockReadyFlag = 0;
    blocks_received_count = 0;
    memset((void*)currentCiphertextBlock, 0, MSG_BYTES);

    ////////////// Enable interrupts /////////////////

    // Enable SYSCFG clock for EXTI configuration
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    
    // Configure EXTICR for PA6 (TX_CLK)
    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI6_Msk;
    SYSCFG->EXTICR[1] |= _VAL2FLD(SYSCFG_EXTICR2_EXTI6, 0b0000); 
    
    // Configure EXTI line 6 for rising edge trigger
    EXTI->IMR1  |= EXTI_IMR1_IM6;   // Unmask EXTI6
    EXTI->RTSR1 |= EXTI_RTSR1_RT6;  // Rising edge trigger
    EXTI->FTSR1 &= ~EXTI_FTSR1_FT6; // Disable falling edge
    
    // Enable NVIC IRQ for EXTI6
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    
    // Global interrupt enable
    __enable_irq();
}

// ISR handler - receives one byte at a time from mechanical system
void msgReceiveISR(void) {
    // Read all bits from GPIO port A at once for speed
    uint32_t bitInput = GPIOA->IDR;

    // Assemble byte from individual bits
    uint8_t byteReceived = 0;
    byteReceived |= ((bitInput >> 0)  & 1) << 0;  // PA0 -> bit 0
    byteReceived |= ((bitInput >> 1)  & 1) << 1;  // PA1 -> bit 1
    byteReceived |= ((bitInput >> 2)  & 1) << 2;  // PA2 -> bit 2
    byteReceived |= ((bitInput >> 3)  & 1) << 3;  // PA3 -> bit 3
    byteReceived |= ((bitInput >> 4)  & 1) << 4;  // PA4 -> bit 4
    byteReceived |= ((bitInput >> 5)  & 1) << 5;  // PA5 -> bit 5
    byteReceived |= ((bitInput >> 7)  & 1) << 6;  // PA7 -> bit 6
    byteReceived |= ((bitInput >> 12) & 1) << 7;  // PA12 -> bit 7

    // Store received byte in current ciphertext block
    currentCiphertextBlock[msgIndex++] = byteReceived;

    // Check if full 16-byte ciphertext block received
    if (msgIndex >= MSG_BYTES) {
        // Block complete! Set flag for main loop to process
        blockReadyFlag = 1;
        msgIndex = 0;  // Reset for next block
    }
}

// EXTI line[9:5] Interrupt Handler
void EXTI9_5_IRQHandler(void) {
    // Check if EXTI6 triggered the interrupt
    if (EXTI->PR1 & EXTI_PR1_PIF6) {
        EXTI->PR1 |= EXTI_PR1_PIF6; // Clear the interrupt flag
        msgReceiveISR();             // Process received byte
    }
}

// Check if a block is ready for decryption
uint8_t isBlockReady(void) {
    return blockReadyFlag;
}

// Get the current ciphertext block (call after isBlockReady() returns 1)
void getCurrentCiphertextBlock(uint8_t buffer[MSG_BYTES]) {
    for (int i = 0; i < MSG_BYTES; i++) {
        buffer[i] = currentCiphertextBlock[i];
    }
}

// Clear the block ready flag (call after processing the block)
void clearBlockReadyFlag(void) {
    blockReadyFlag = 0;
    memset((void*)currentCiphertextBlock, 0, MSG_BYTES);
}

// Increment block counter and update LCD
void incrementBlockCount(void) {
    blocks_received_count++;
    on_block_received(&g_lcd);  // Update LCD display
}

// Public function to get current block count
uint16_t getMsgReceivedBlockCount(void) {
    return blocks_received_count;
}

// Public function to reset receiver (call when starting new transfer)
void resetMsgReceive(void) {
    blocks_received_count = 0;
    msgIndex = 0;
    blockReadyFlag = 0;
    memset((void*)currentCiphertextBlock, 0, MSG_BYTES);
}