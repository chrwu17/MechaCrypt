#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "STM32L432KC.h"
#include "STM32L432KC_GPIO.h"
#include "STM32L432KC_USART.h"
#include "STM32L432KC_TIM.h"

// System-level constants
#define LED_PIN   PB3
#define READY_PIN PA5
#define VALID_PIN PA6

#define MAX_BLOCKS     64           // number of blocks UI can stage
#define BUFF_LEN       512          // HTTP request line buffer

// System bring-up
void configureFlash(void);
void configureClock(void);

// Delay
void delay_us(uint32_t us);

#endif
