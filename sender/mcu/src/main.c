/*
 * File: main.c
 * Mode: LED-only status, TRNG + SPI + LOAD/DONE handshake
 *
 * Flow:
 *  - Boot: short LED blinks to verify timer feels right.
 *  - TRNG init: 2 long blinks on success; 3 fast blinks on failure (still runs).
 *  - Web requests: /send stores plaintext, generates a TRNG key per block.
 *  - SPI send:
 *      * First block is sent immediately when its /send occurs.
 *      * Thereafter, each DONE (PA6) from the FPGA triggers the next block send.
 *
 * No debug prints, no changes to GPIO/USART/SPI driver files.
 */

#include "../lib/main.h"
#include "../lib/webpage.h"
#include "../lib/STM32L432KC.h"
#include "../lib/STM32L432KC_TIM.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/trng.h"

static inline void led_on(void){  digitalWrite(LED_PIN, 1); }
static inline void led_off(void){ digitalWrite(LED_PIN, 0); }

// Small visual cue helpers
static void blink_short(uint32_t ms){ led_on();  delay_millis(TIM15, ms); led_off(); delay_millis(TIM15, ms); }
static void blink_count(uint32_t on_ms, uint32_t off_ms, int n){
  for(int i=0;i<n;i++){ led_on(); delay_millis(TIM15, on_ms); led_off(); delay_millis(TIM15, off_ms); }
}

int main(void) {
  // Core clocks and flash latency
  configureFlash();
  configureClock();

  // Enable GPIO ports (A/B/C)
  gpioEnable(GPIO_PORT_A);
  gpioEnable(GPIO_PORT_B);
  gpioEnable(GPIO_PORT_C);

  // LED pin as output
  pinMode(LED_PIN, GPIO_OUTPUT);

  // Timer for millisecond delays (TIM15)
  initTIM(TIM15);

  // Power-on sanity blinks (confirm delays feel like real ms)
  blink_short(200);
  blink_short(200);

  // Bring up USART for the web server (no debug prints)
  USART_TypeDef *USART = initUSART(USART1_ID, 125000);

  // Bring up TRNG (LED-only status)
  int trng_status = initTRNG();
  if (trng_status == 0) {
    // TRNG OK: 2 long blinks
    blink_count(500, 500, 2);
  } else {
    // TRNG FAIL: 3 fast blinks (keep running; /send will fail)
    blink_count(100, 100, 3);
  }

  // Configure SPI and handshake pins (LOAD/DONE)
  mechacrypt_init_io_and_spi();

  // Main loop:
  //  - service HTTP
  //  - poll DONE and advance queued block sends (if any)
  while (1) {
    processWebRequest(USART);
    mechacrypt_poll_and_advance();
  }
}
