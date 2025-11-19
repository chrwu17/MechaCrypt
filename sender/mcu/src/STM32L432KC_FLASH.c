/**
 * @file STM32L432KC_FLASH.c
 * @author Christian Wu
 * @date 2025-09-30
 * @brief Source code for FLASH functions. Taken from the E155 Course Website
 */

#include "../lib/STM32L432KC_FLASH.h"

void configureFlash() {
  FLASH->ACR |= FLASH_ACR_LATENCY_4WS;
  FLASH->ACR |= FLASH_ACR_PRFTEN;
}