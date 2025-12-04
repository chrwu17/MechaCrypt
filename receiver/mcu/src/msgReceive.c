/**
 * @file msgReceive.c
 * @author 
 * @date  2025-11-19
 * @brief Mechanical-safe message reception using EXTI rising edge + TIM2 delayed sampling.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32l432xx.h"
#include "../lib/STM32L432KC.h"
#include "../lib/msgReceive.h"
#include "../lib/webpage.h"
#include "../lib/lcd_progress.h"
#include "../lib/main.h"

// Debug print support (ITM)
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        ITM_SendChar((*ptr++));
    }
    return len;
}


// ===================
// Global Receiver State
// ===================
volatile uint8_t receivedMessage[MSG_BYTES];
volatile uint8_t messageReceivedFlag = 0;
volatile uint8_t msgIndex = 0;
volatile uint32_t totalBlocksReceived = 0;

// Debug counters
volatile uint32_t interrupt_count = 0;
volatile uint32_t rejected_count = 0;
volatile uint32_t debounced_count = 0;
volatile uint32_t last_byte_value = 0;

// Timing helpers
volatile uint32_t last_interrupt_time = 0;

// Timer-driven sampling state
volatile uint8_t sample_pending = 0;

// DWT timestamp (1 µs resolution)
static inline uint32_t get_us_timestamp(void) {
    return DWT->CYCCNT / 80;   // 80 MHz → 80 cycles/us
}


// ==========================
// INIT FUNCTION
// ==========================
void initMsgReceive(void) {

    // ---- Enable DWT cycle counter ----
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // ---- Configure BUS GPIO inputs ----
    pinMode(BIT_0, GPIO_INPUT);
    pinMode(BIT_1, GPIO_INPUT);
    pinMode(BIT_2, GPIO_INPUT);
    pinMode(BIT_3, GPIO_INPUT);
    pinMode(BIT_4, GPIO_INPUT);
    pinMode(BIT_5, GPIO_INPUT);
    pinMode(BIT_6, GPIO_INPUT);
    pinMode(BIT_7, GPIO_INPUT);
    pinMode(TX_CLK, GPIO_INPUT);  // PA2 rising edge

    // ---- Clear pull-ups/pull-downs ----

    // ---- Enable pull-downs on all data pins ----
    // Clear first, then set to 0b10 (pull-down)
    GPIOA->PUPDR &= ~(0b11 << (7*2));
    GPIOA->PUPDR |= (0b10 << (7*2));   // PA7 pull-down

    GPIOA->PUPDR &= ~(0b11 << (6*2));
    GPIOA->PUPDR |= (0b10 << (6*2));   // PA6 pull-down

    GPIOA->PUPDR &= ~(0b11 << (3*2));
    GPIOA->PUPDR |= (0b10 << (3*2));   // PA3 pull-down

    GPIOA->PUPDR &= ~(0b11 << (1*2));
    GPIOA->PUPDR |= (0b10 << (1*2));   // PA1 pull-down

    GPIOA->PUPDR &= ~(0b11 << (0*2));
    GPIOA->PUPDR |= (0b10 << (0*2));   // PA0 pull-down

    GPIOB->PUPDR &= ~(0b11 << (1*2));
    GPIOB->PUPDR |= (0b10 << (1*2));   // PB1 pull-down

    GPIOC->PUPDR &= ~(0b11 << (14*2));
    GPIOC->PUPDR |= (0b10 << (14*2));  // PC14 pull-down

    GPIOC->PUPDR &= ~(0b11 << (15*2));
    GPIOC->PUPDR |= (0b10 << (15*2));  // PC15 pull-down

    // ==========================
    // EXTI2 Rising Edge Interrupt
    // ==========================
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI2_Msk;
    SYSCFG->EXTICR[0] |= _VAL2FLD(SYSCFG_EXTICR1_EXTI2, 0b0000);  // PA2

    EXTI->IMR1  |= EXTI_IMR1_IM2;
    EXTI->RTSR1 |= EXTI_RTSR1_RT2;
    EXTI->FTSR1 &= ~EXTI_FTSR1_FT2;

    NVIC_EnableIRQ(EXTI2_IRQn);
    NVIC_SetPriority(EXTI2_IRQn, 1);

    // ==========================
    // TIM2 — Delayed Sampling Timer (10 ms)
    // ==========================
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    TIM2->PSC = 80 - 1;       // 80MHz → 1MHz, 1us tick
    TIM2->ARR = 10000;        // 10,000us = 10ms delay
    TIM2->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM2_IRQn);

    // Reset state
    msgIndex = 0;
    messageReceivedFlag = 0;
    totalBlocksReceived = 0;
    interrupt_count = 0;
    rejected_count = 0;
    debounced_count = 0;
    last_interrupt_time = 0;
    sample_pending = 0;

    __enable_irq();
}


// ==========================
// EXTI2 — Rising Edge Detected
// ==========================
void EXTI2_IRQHandler(void) {
    if (EXTI->PR1 & EXTI_PR1_PIF2) {

        EXTI->PR1 |= EXTI_PR1_PIF2;  // Clear interrupt

        uint32_t now = get_us_timestamp();

        // Block extra events for 12ms
        if (now - last_interrupt_time < 12000) {
            rejected_count++;
            return;
        }

        interrupt_count++;

        // Disable further EXTI2 triggers until sampling done
        EXTI->IMR1 &= ~EXTI_IMR1_IM2;

        // Start delayed sampling
        sample_pending = 1;
        last_interrupt_time = now;

        // Reset and start timer
        TIM2->CNT = 0;
        TIM2->CR1 |= TIM_CR1_CEN;
    }
}


// ==========================
// TIM2 — Perform Sampling (10 ms later)
// ==========================
void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        TIM2->CR1 &= ~TIM_CR1_CEN;

        if (sample_pending) {
            sample_pending = 0;

            // Take 3 samples, 2ms apart
            uint8_t samples[3];
            for (int i = 0; i < 3; i++) {
                uint32_t porta = GPIOA->IDR;
                uint32_t portb = GPIOB->IDR;
                uint32_t portc = GPIOC->IDR;

                samples[i] = 0;
                samples[i] |= ((porta >> 7) & 1) << 0;
                samples[i] |= ((porta >> 4) & 1) << 1;
                samples[i] |= ((porta >> 3) & 1) << 2;
                samples[i] |= ((porta >> 1) & 1) << 3;
                samples[i] |= ((porta >> 0) & 1) << 4;
                samples[i] |= ((portb >> 1) & 1) << 5;
                samples[i] |= ((portc >> 14) & 1) << 6;
                samples[i] |= ((portc >> 15) & 1) << 7;
                
                if (i < 2) delay_us(2000);  // 2ms between samples
            }

            // Only accept if all 3 samples match
            if (samples[0] == samples[1] && samples[1] == samples[2]) {
                last_byte_value = samples[0];
                receivedMessage[msgIndex++] = samples[0];
                debounced_count++;

                if (msgIndex >= MSG_BYTES) {
                    msgIndex = 0;
                    messageReceivedFlag = 1;
                    totalBlocksReceived++;
                }
            } else {
                rejected_count++;
            }
        }

        EXTI->PR1 |= EXTI_PR1_PIF2;
        EXTI->IMR1 |= EXTI_IMR1_IM2;
    }
}


// ==========================
// Helper Functions
// ==========================
uint32_t getTotalBlocksReceived(void) { return totalBlocksReceived; }
uint8_t  getCurrentByteIndex(void)   { return msgIndex; }
uint32_t getInterruptCount(void)     { return interrupt_count; }
uint32_t getRejectedCount(void)      { return rejected_count; }
uint32_t getDebouncedCount(void)     { return debounced_count; }
uint8_t  getLastByteValue(void)      { return (uint8_t)last_byte_value; }

void resetMsgReceive(void) {
    msgIndex = 0;
    messageReceivedFlag = 0;
    totalBlocksReceived = 0;
    interrupt_count = 0;
    rejected_count = 0;
    debounced_count = 0;
    last_interrupt_time = 0;
    sample_pending = 0;
    memset((void*)receivedMessage, 0, MSG_BYTES);
}
