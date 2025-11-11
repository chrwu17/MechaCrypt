// Christian Wu
// chrwu@g.hmc.edu
// 09/30/25

// Taken from the E155 Course Website

// STM32L432KC_FLASH.c
// Source code for FLASH functions

#include "../lib/STM32L432KC_FLASH.h"

void configureFlash() {
  FLASH->ACR |= FLASH_ACR_LATENCY_4WS;
  FLASH->ACR |= FLASH_ACR_PRFTEN;
}