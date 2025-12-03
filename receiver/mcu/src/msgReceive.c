/**
 * @file msgReceive.c
 * @author Josaphat Ngoga / Christian Wu
 * @date  2025-11-19
 * @brief Enhanced message reception with debouncing
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../lib/STM32L432KC.h"
#include "../lib/msgReceive.h"
#include "../lib/webpage.h"
#include "../lib/lcd_progress.h"

// Variables
volatile uint8_t receivedMessage[MSG_BYTES];
volatile uint8_t messageReceivedFlag = 0;
volatile uint8_t msgIndex = 0;
volatile uint32_t totalBlocksReceived = 0;

// Debug counters
volatile uint32_t interrupt_count = 0;
volatile uint32_t debounced_count = 0;  // Count after debouncing
volatile uint32_t rejected_count = 0;   // Count of rejected (too fast) interrupts
volatile uint32_t last_byte_value = 0;

// Debouncing state
volatile uint32_t last_interrupt_time = 0;  // Using systick or timer
#define DEBOUNCE_TIME_US 10000  // 1ms debounce (adjust based on testing)

// Initialize the receiver GPIO pins and interrupt
void initMsgReceive(void) {
    // Configure GPIO inputs
    pinMode(BIT_0, GPIO_INPUT);
    pinMode(BIT_1, GPIO_INPUT);
    pinMode(BIT_2, GPIO_INPUT);
    pinMode(BIT_3, GPIO_INPUT);
    pinMode(BIT_4, GPIO_INPUT);
    pinMode(BIT_5, GPIO_INPUT);
    pinMode(BIT_6, GPIO_INPUT);
    pinMode(BIT_7, GPIO_INPUT);

    // Configure TX_CLK input
    pinMode(TX_CLK, GPIO_INPUT);

    // Initialize counters
    msgIndex = 0;
    messageReceivedFlag = 0;
    totalBlocksReceived = 0;
    interrupt_count = 0;
    debounced_count = 0;
    rejected_count = 0;
    last_interrupt_time = 0;

    ////////////// Enable interrupts /////////////////

    // Enable SYSCFG clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    
    // Configure EXTICR for PA0 (TX_CLK)
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0_Msk;
    SYSCFG->EXTICR[0] |= _VAL2FLD(SYSCFG_EXTICR1_EXTI0, 0b0000);
    
    // Configure EXTI line 0 for rising edge trigger ONLY
    EXTI->IMR1  |= EXTI_IMR1_IM0;     // Unmask EXTI0
    EXTI->RTSR1 |= EXTI_RTSR1_RT0;    // Rising edge trigger
    EXTI->FTSR1 &= ~EXTI_FTSR1_FT0;   // Disable falling edge (important!)
    
    // Enable NVIC IRQ for EXTI0
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_SetPriority(EXTI0_IRQn, 1);  // Higher priority (lower number)
    
    // Global interrupt enable
    __enable_irq();
}

// Simple microsecond timestamp (wraps every ~4s at 80MHz)
static inline uint32_t get_us_timestamp(void) {
    // Using DWT cycle counter (if available)
    // Or use a dedicated timer - for now, use a simple approximation
    return DWT->CYCCNT / 80;  // 80 MHz / 80 = 1 MHz = 1us
}

// ISR handler for message receiving with debouncing
void msgReceiveISR(void) {
    interrupt_count++;
    
    // Software debouncing: ignore interrupts that come too quickly
    uint32_t now = get_us_timestamp();
    uint32_t time_since_last = now - last_interrupt_time;
    
    // If less than DEBOUNCE_TIME_US has passed, ignore this interrupt
    if (time_since_last < DEBOUNCE_TIME_US && last_interrupt_time != 0) {
        rejected_count++;
        return;  // Ignore this bounce
    }
    
    last_interrupt_time = now;
    debounced_count++;
    
    // Read all GPIO Port A pins at once for speed
    uint32_t bitInput = GPIOA->IDR;

    // Reconstruct the byte from the 8 bit lines
    uint8_t byteReceived = 0;
    
    byteReceived |= ((GPIOA->IDR >> 7) & 1) << 0;   // PA7  -> bit 0 ✓
    byteReceived |= ((GPIOA->IDR >> 6) & 1) << 1;   // PA6  -> bit 1 ✓
    byteReceived |= ((GPIOA->IDR >> 3) & 1) << 2;   // PA3  -> bit 2 ✓
    byteReceived |= ((GPIOA->IDR >> 1) & 1) << 3;   // PA1  -> bit 3 ✓
    byteReceived |= ((GPIOA->IDR >> 0) & 1) << 4;   // PA0  -> bit 4 ✓
    byteReceived |= ((GPIOB->IDR >> 1) & 1) << 5;   // PB1  -> bit 5 ✓
    byteReceived |= ((GPIOC->IDR >> 14) & 1) << 6;  // PC14 -> bit 6 ✓
    byteReceived |= ((GPIOC->IDR >> 15) & 1) << 7;  // PC15 -> bit 7 ✓

    last_byte_value = byteReceived;

    // Store received byte
    receivedMessage[msgIndex++] = byteReceived;

    // Check if full 16-byte block received
    if (msgIndex >= MSG_BYTES) {
        messageReceivedFlag = 1;
        msgIndex = 0;
        totalBlocksReceived++;
    }
}

// EXTI line 0 Interrupt Handler (PA0 = TX_CLK)
void EXTI0_IRQHandler(void) {
    // Check if EXTI0 triggered the interrupt
    if (EXTI->PR1 & EXTI_PR1_PIF0) {
        EXTI->PR1 |= EXTI_PR1_PIF0;  // Clear the interrupt flag IMMEDIATELY
        msgReceiveISR();
    }
}

// Helper functions
uint32_t getTotalBlocksReceived(void) {
    return totalBlocksReceived;
}

uint8_t getCurrentByteIndex(void) {
    return msgIndex;
}

uint32_t getInterruptCount(void) {
    return interrupt_count;
}

uint32_t getDebouncedCount(void) {
    return debounced_count;
}

uint32_t getRejectedCount(void) {
    return rejected_count;
}

uint8_t getLastByteValue(void) {
    return (uint8_t)last_byte_value;
}

void resetMsgReceive(void) {
    msgIndex = 0;
    messageReceivedFlag = 0;
    totalBlocksReceived = 0;
    interrupt_count = 0;
    debounced_count = 0;
    rejected_count = 0;
    last_interrupt_time = 0;
    memset((void*)receivedMessage, 0, MSG_BYTES);
}