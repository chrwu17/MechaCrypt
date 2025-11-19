/**
 * @file trng.c
 * @author Christian Wu
 * @date 2024-11-19
 * @brief Source file for True Random Number Generator (TRNG) functions.
 */

#include "../lib/trng.h"
#include <stm32l432xx.h>

/* --------- Portable error-bit detection --------- */
#if defined(RNG_SR_SEIS)
  #define RNG_SEED_ERR_BIT   (RNG_SR_SEIS)
#elif defined(RNG_SR_SECS)
  #define RNG_SEED_ERR_BIT   (RNG_SR_SECS)
#else
  #define RNG_SEED_ERR_BIT   (0U)
#endif

#if defined(RNG_SR_CEIS)
  #define RNG_CLK_ERR_BIT    (RNG_SR_CEIS)
#elif defined(RNG_SR_CECS)
  #define RNG_CLK_ERR_BIT    (RNG_SR_CECS)
#else
  #define RNG_CLK_ERR_BIT    (0U)
#endif

#define RNG_ANY_ERR_MASK     (RNG_SEED_ERR_BIT | RNG_CLK_ERR_BIT)

/* --------- Portable CCIPR bit definitions (CLK48SEL at bits 27:26) --------- */
#ifndef RCC_CCIPR_CLK48SEL_Pos
  #define RCC_CCIPR_CLK48SEL_Pos  (26U)
#endif
#ifndef RCC_CCIPR_CLK48SEL_Msk
  #define RCC_CCIPR_CLK48SEL_Msk  (3U << RCC_CCIPR_CLK48SEL_Pos)
#endif

/* Select HSI48 for CLK48 (00 on many L4 parts). If your part uses a different mapping,
   clearing the field generally selects HSI48; otherwise adjust as needed. */
static int enable_hsi48_and_route_clk48(void) {
  /* Turn on HSI48 */
  RCC->CRRCR |= RCC_CRRCR_HSI48ON;
  /* Wait until ready (short timeout) */
  for (volatile uint32_t t = 0; t < 200000; ++t) {
    if (RCC->CRRCR & RCC_CRRCR_HSI48RDY) break;
    if (t == 199999) return -1;  // HSI48 failed to start
    __NOP();
  }

  /* Route CLK48 = HSI48: clear CLK48SEL field */
  RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL_Msk;

  return 0;
}

/* Simple delay a few cycles (not timing critical) */
static inline void short_delay(void) {
  for (volatile int i = 0; i < 256; ++i) { __NOP(); }
}

/* Error recovery by toggling RNGEN (portable across L4 variants) */
static inline void rng_recover(void) {
  RNG->CR &= ~RNG_CR_RNGEN;  // disable
  short_delay();
  RNG->CR |= RNG_CR_RNGEN;   // enable
}

/* Public: initialize RNG */
int initTRNG(void) {
  /* Provide 48 MHz clock domain */
  if (enable_hsi48_and_route_clk48() != 0) {
    return -10;  // HSI48 failed to start
  }

  /* Enable RNG peripheral clock on AHB2 */
  RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;

  /* Small stabilization delay */
  for (volatile int i = 0; i < 1000; ++i) __NOP();

  /* Ensure RNG disabled, then enable cleanly */
  RNG->CR &= ~RNG_CR_RNGEN;
  short_delay();
  RNG->CR |= RNG_CR_RNGEN;

  /* Wait for first DRDY with timeout, recover from errors if present */
  uint32_t timeout = 0;
  while ((RNG->SR & RNG_SR_DRDY) == 0U) {
    if ((RNG_ANY_ERR_MASK != 0U) && (RNG->SR & RNG_ANY_ERR_MASK)) {
      rng_recover();
    }
    if (++timeout > 200000U) {
      RNG->CR &= ~RNG_CR_RNGEN;
      return -11;  // timed out waiting for DRDY
    }
    __NOP();
  }

  /* Optional "prime" read */
  (void)RNG->DR;

  return 0;
}

/* Public: read 128 bits into buffer (16 bytes) */
int read_trng(uint8_t *buffer) {
  if (!buffer) return -1;
  if ((RNG->CR & RNG_CR_RNGEN) == 0U) return -3; // RNG not enabled

  uint32_t *out32 = (uint32_t*)buffer;

  for (int i = 0; i < 4; ++i) {
    /* Wait for DRDY with timeout, recover if error set */
    uint32_t t = 0;
    while ((RNG->SR & RNG_SR_DRDY) == 0U) {
      if ((RNG_ANY_ERR_MASK != 0U) && (RNG->SR & RNG_ANY_ERR_MASK)) {
        rng_recover();
      }
      if (++t > 200000U) {
        return -2;  // timeout
      }
      __NOP();
    }

    /* Read word; very rarely DR could be zero immediately after recovery—retry */
    uint32_t rnd = RNG->DR;
    if (rnd == 0U) { --i; continue; }

    out32[i] = rnd;
  }

  return 0;
}
