#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <stdint.h>
#include "STM32L432KC_USART.h"

// HTML server & request handler
void processWebRequest(USART_TypeDef *USART);

// SPI + handshake (exposed to main.c)
void mechacrypt_init_io_and_spi(void);
void mechacrypt_poll_and_advance(void);
void mechacrypt_maybe_start_after_block(int block_idx);

#endif // WEBPAGE_H
