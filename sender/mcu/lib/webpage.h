#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <stdint.h>
#include <stm32l432xx.h>

// The static page we serve
extern const char webpage[];

// Minimal HTTP handler (blocking, reads one request line and responds)
void processWebRequest(USART_TypeDef *USART);

#endif