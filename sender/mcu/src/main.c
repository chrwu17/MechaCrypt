/**
 * @file main.c
 * @author Christian Wu
 * @date 2024-11-19
 * @brief Main file for MechaCrypt sender MCU.
 */

#include "../lib/main.h"

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

  // Bring up USART for the web server
  USART_TypeDef *USART = initUSART(USART1_ID, 125000);

  // Init TRNG
  initTRNG();

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
