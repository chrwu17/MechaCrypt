/**
 * @file STM32L432KC_RCC.h
 * @author Christian Wu
 * @date 2024-11-19
 * @brief Header file for RCC functions for STM32L432KC microcontroller.
 */

#ifndef STM32L4_RCC_H
#define STM32L4_RCC_H

#include <stdint.h>
#include <stm32l432xx.h>

///////////////////////////////////////////////////////////////////////////////
// Function prototypes
///////////////////////////////////////////////////////////////////////////////

void configurePLL();
void configureClock();

#endif