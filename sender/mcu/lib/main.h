// -----------------------------------------------------------------------------
// main.h  —  MechaCrypt Sender (STM32L432KC, CMSIS-only, no HAL/LL)
// Central app definitions + prototypes used by main.c / webpage.c
// -----------------------------------------------------------------------------
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stddef.h>
#include <stm32l432xx.h>
#include "STM32L432KC.h"
#include "webpage.h"

// ===== App constants =====

// Length for reading a single HTTP request line from ESP8266 over UART.
// (Plenty for "GET /send?i=..&hex=.. HTTP/1.1\r\n")
#define BUFF_LEN        256

// On NUCLEO-L432KC, the user LED (LD3) is PB3.
// If your LED is different, change this to your pin macro used by your GPIO lib.
#define LED_PIN         ( (uint32_t)0x00030000u ) 
// ^ If your GPIO library uses its own pin macros, replace the above with that.
//   Example alternatives your project might use:
//   #define LED_PIN PIN_PB3
//   #define LED_PIN PB3
//   #define LED_PIN GPIO_PB3

// GPIO port IDs expected by your gpioEnable(...) helper
#define GPIO_PORT_A     0
#define GPIO_PORT_B     1
#define GPIO_PORT_C     2

// pinMode() modes (match your GPIO library)
#define GPIO_OUTPUT     1
#define GPIO_INPUT      0



#endif // MAIN_H
