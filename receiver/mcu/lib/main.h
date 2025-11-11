/* ==========================
 * File: main.h
 * ========================== */
#ifndef PROJECT_MAIN_H
#define PROJECT_MAIN_H

#include <stdint.h>
#include "STM32L432KC.h"

// Clock/Flash helpers provided in your BSP (keep prototypes for linker)
void configureFlash(void);
void configureClock(void);

// Minimal GPIO helpers (provided by your GPIO library)
void gpioEnable(int port);            // e.g., GPIO_PORT_A
void pinMode(int pin, int mode);      // e.g., OUTPUT, INPUT
void digitalWrite(int pin, int val);  // 0/1

// Simple timing (if available)
void delay_ms(uint32_t ms);

// USART helpers (provided by your USART lib)
void initUSART(USART_TypeDef *USART, uint32_t baud);
void sendString(USART_TypeDef *USART, const char *s);
void sendChar(USART_TypeDef *USART, char c);
char readChar(USART_TypeDef *USART);

#endif // PROJECT_MAIN_H
