/*
 * File: main.c
 * Author: MechaCrypt - LED-only status (no UART/RTT debug prints)
 *
 * LED patterns:
 *   - Power-on check: 2 × 200 ms blinks (verifies timer is sane)
 *   - TRNG OK:        2 × 500 ms blinks
 *   - TRNG FAIL:      3 × 100 ms blinks
 *
 * Notes:
 *   - Assumes TIM15 delay_millis() uses a correct ms timebase (fixed in STM32L432KC_TIM.c).
 *   - Assumes trng.c enables HSI48 and selects CLK48 correctly before enabling RNG.
 *   - USART1 remains used for the web server only; no prints are emitted.
 */

#include "../lib/main.h"
#include "../lib/webpage.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_TIM.h"
#include "../lib/STM32L432KC.h"
#include "../lib/trng.h"

int main(void) {
  // Core clocks and flash latency
  configureFlash();
  configureClock();

  // Enable GPIO ports
  gpioEnable(GPIO_PORT_A);
  gpioEnable(GPIO_PORT_B);
  gpioEnable(GPIO_PORT_C);

  // LED pin as output
  pinMode(LED_PIN, GPIO_OUTPUT);

  // Timer for millisecond delays (TIM15)
  initTIM(TIM15);

  // Bring up USART for the web server (no debug prints)
  USART_TypeDef *USART = initUSART(USART1_ID, 125000);

  // TRNG initialization (LED-only status)
  int trng_status = initTRNG();
  if (trng_status == 0) {
    // TRNG OK: 2 long blinks
    for (int j = 0; j < 2; j++) {
      digitalWrite(LED_PIN, 1);
      delay_millis(TIM15, 500);
      digitalWrite(LED_PIN, 0);
      delay_millis(TIM15, 500);
    }
  } else {
    // TRNG FAIL: 3 fast blinks
    for (int j = 0; j < 3; j++) {
      digitalWrite(LED_PIN, 1);
      delay_millis(TIM15, 100);
      digitalWrite(LED_PIN, 0);
      delay_millis(TIM15, 100);
    }
    // Keep running: webpage /send will return 500 if TRNG isn't working.
  }

  // Main loop: serve page and handle /send requests
  while (1) {
    processWebRequest(USART);
  }
}
