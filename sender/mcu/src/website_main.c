/*
File: main.c
Author: MechaCrypt (no-debug variant)
*/
#include "../lib/main.h"

int main(void) {
  configureFlash();
  configureClock();

  gpioEnable(GPIO_PORT_A);
  gpioEnable(GPIO_PORT_B);
  gpioEnable(GPIO_PORT_C);

  pinMode(LED_PIN, GPIO_OUTPUT);   // used for blink ACK

  initTIM(TIM15);

  // Keep your existing UART link to ESP8266 (same baud you already use)
  USART_TypeDef *USART = initUSART(USART1_ID, 125000);

  while (1) {
    processWebRequest(USART);  // serve page / accept /send? blocks
  }
}
