/**
 * @file STM32L432KC_RCC.h
 * @author Christian Wu
 * @date 2025-09-30
 * @brief Header for RCC functions. Taken from the E155 Course Website
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