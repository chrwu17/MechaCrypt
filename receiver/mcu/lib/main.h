/**
 * @file main.h
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Header file for main file
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "STM32L432KC.h"
#include "lcd_i2c.h"
#include "webpage.h"
#include "lcd_progress.h"
#include "msgReceive.h"
#include "fpga_decrypt.h"
// #include "mcu_to_mcu_spi.h"

// System-level constants
#define LED_PIN   PA8 // Debug LED

// // SPI Transaction Pins
// #define READY_PIN PA5
// #define VALID_PIN PA6



// System bring-up
void configureFlash(void);
void configureClock(void);

// Delay
void delay_us(uint32_t us);

uint32_t get_millis(void);

#endif
