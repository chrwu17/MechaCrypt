/**
 * @file msgReceive.c
 * @author Josaphat Ngoga
 * @date  19/11/2025
 * @brief Reads the bit transfer lines from the switches and recods the full byte as received. It is ISR driven where it
 * triggers on the rising edge of the transfre clk line (tx_clk) stores them and sets a flag when 16 bytes (128 bits) are received.
*/

#include <stdint.h>
#include <stdio.h>
#include "../lib/STM32L432KC.h"
#include "../lib/msgReceive.h"

// Variables
volatile uint8_t receivedMessage[MSG_BYTES]; // Arraty to store received message
volatile uint8_t messageReceivedFlag = 0;    // Initialize flag
volatile uint8_t msgIndex;                   // Byte counter

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

    // Configure TX_CLK 
    pinMode(TX_CLK, GPIO_INPUT);

    // Initialize signals
    msgIndex = 0;
    messageReceivedFlag = 0;


    ////////////// Enable interrupts /////////////////

    // Enable SYSCFG clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    
    // Configure EXTICR for PA6
    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI6_Msk; // Clear EXTI6 bits
    SYSCFG->EXTICR[1] |= _VAL2FLD(SYSCFG_EXTICR2_EXTI6, 0b0000); 
    
    // Configure EXTI line 6 for rising edge trigger
    EXTI->IMR1  |= EXTI_IMR1_IM6;   // Unmask EXTI6
    EXTI->RTSR1 |= EXTI_RTSR1_RT6;  // Rising edge trigger
    EXTI->FTSR1 &= ~EXTI_FTSR1_FT6; // Disable falling edge trigger
    
    // Enable NVIC IRQ for EXTI6
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    // Global interrupt enable
    __enable_irq();
    
}

// ISR handler for message receiving
void msgReceiveISR(void) {
    // Configure GPIO IDR for faster reading
    uint32_t bitInput = GPIOA->IDR;

    // Read bits from GPIO pins
    uint8_t byteReceived = 0;
    
    byteReceived |= ((bitInput >> 0)  & 1) << 0;
    byteReceived |= ((bitInput >> 1)  & 1) << 1;
    byteReceived |= ((bitInput >> 2)  & 1) << 2;
    byteReceived |= ((bitInput >> 3)  & 1) << 3;
    byteReceived |= ((bitInput >> 4)  & 1) << 4;
    byteReceived |= ((bitInput >> 5)  & 1) << 5;
    byteReceived |= ((bitInput >> 7)  & 1) << 6;
    byteReceived |= ((bitInput >> 12) & 1) << 7;

    // Store received byte
    receivedMessage[msgIndex++] = byteReceived;

    // Check if full message received
    if (msgIndex >= MSG_BYTES) {
        messageReceivedFlag = 1; // Set flag
        msgIndex = 0;            // Reset index for next message
    }
}

// EXTI line[9:5] Interrupt Handler
void EXTI9_5_IRQHandler(void) {

    // Check if EXTI6 triggered the interrupt
    if (EXTI->PR1 & EXTI_PR1_PIF6) {
        EXTI->PR1 |= EXTI_PR1_PIF6; // Clear the interrupt
        msgReceiveISR();            // Read message bits
    }
}