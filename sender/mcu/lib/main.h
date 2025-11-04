// Christian Wu
// chrwu@g.hmc.edu
// 10/17/25

// Header file for main.c

#ifndef MAIN_H
#define MAIN_H

#include "STM32L432KC.h"
#include <stm32l432xx.h>

#include "STM32L432KC_FLASH.h"
#include "STM32L432KC_GPIO.h"
#include "STM32L432KC_RCC.h"
#include "STM32L432KC_SPI.h"
#include "STM32L432KC_TIM.h"
#include "STM32L432KC_USART.h"
#include "lcd.h"

#define MCK_FREQ 100000
#define LOAD_PIN PA5
#define DONE_PIN PA6
#define SUCCESS_LED PA9
#define FAIL_LED PA10
#define SPI_CE PA11

#endif