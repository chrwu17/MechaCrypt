/**
 * @file STM32L432KC_TIM.c
 * @author Christian Wu
 * @date 2025-09-30
 * @brief Source code for TIM functions. Taken from the E155 Course Website
 */

#include "../lib/STM32L432KC_TIM.h"
#include "../lib/STM32L432KC_RCC.h"

#define TIM_BASE_HZ 100000u  // 100 kHz tick (10 us per tick)

void initTIM(TIM_TypeDef * TIMx){
  // Enable TIM15 clock (APB2)
  RCC->APB2ENR |= (RCC_APB2ENR_TIM15EN);

  // Prescale to ~100 kHz: PSC = (SystemCoreClock / TIM_BASE_HZ) - 1
  uint32_t psc = (uint32_t)(SystemCoreClock / TIM_BASE_HZ);
  if (psc == 0) psc = 1;     // safety
  TIMx->PSC = (uint16_t)(psc - 1);

  TIMx->EGR |= 1;            // UG: latch PSC
  TIMx->CR1 |= 1;            // CEN
}

// Blocks for 'ms' milliseconds by using 100 kHz base (ARR in 'ticks' = ms * 100)
void delay_millis(TIM_TypeDef * TIMx, uint32_t ms){
  while (ms) {
    // handle up to ~655 ms per chunk at 100 kHz
    uint32_t chunk_ms = (ms > 655) ? 655 : ms;
    uint32_t ticks = chunk_ms * (TIM_BASE_HZ / 1000u); // ms * 100 = ticks

    TIMx->ARR = (uint16_t)ticks;
    TIMx->EGR |= 1;     // UG
    TIMx->SR &= ~(0x1); // clear UIF
    TIMx->CNT = 0;

    while(!(TIMx->SR & 1)) { /* wait */ }

    ms -= chunk_ms;
  }
}
