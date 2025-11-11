// Christian Wu
// chrwu@g.hmc.edu
// 09/30/25

// Taken from the E155 Course Website

// STM32L432KC_RCC.h
// Header for RCC functions

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