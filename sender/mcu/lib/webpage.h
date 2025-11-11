// MechaCrypt Sender: static webpage over UART (PKCS#7 client-side tool)
#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <stdint.h>
#include <stm32l432xx.h>

// Single static page (sent on every request)
extern const char webpage[];

// Minimal request handler: wait for one line, then send the page
void processWebRequest(USART_TypeDef *USART);

#endif
