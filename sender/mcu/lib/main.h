#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stddef.h>
#include <stm32l432xx.h>
#include "STM32L432KC.h"
#include "webpage.h"

// ===== App constants =====

#define BUFF_LEN        256

// LED Pin - using GPIO library pin numbering (PB3 = 19)
#define LED_PIN PB3   // or just use 19 if PB3 isn't defined

#define MAX_BLOCKS      100

#define GPIO_PORT_A     0
#define GPIO_PORT_B     1
#define GPIO_PORT_C     2

#define GPIO_OUTPUT     1
#define GPIO_INPUT      0

// ===== Shared global variables =====
extern uint8_t plaintext_blocks[MAX_BLOCKS][16];
extern uint8_t keys[MAX_BLOCKS][16];
extern uint8_t have_block[MAX_BLOCKS];
extern volatile uint16_t total_blocks;

#endif // MAIN_H