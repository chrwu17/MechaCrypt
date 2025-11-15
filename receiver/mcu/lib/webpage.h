#ifndef WEBPAGE_H
#define WEBPAGE_H

#include "stm32l4xx.h"
#include <stdint.h>

#define MAX_BLOCKS 64
#define BUFF_LEN   512

#ifdef __cplusplus
extern "C" {
#endif

// Init all webpage + SPI logic
void web_init(void);

// Poll HTTP traffic (USART1)
void web_poll_uart(USART_TypeDef *USART);

// Poll SPI receiver
void web_poll_spi(void);

// Store 16-byte block
void store_received_block(int block_idx, const uint8_t *block16);

#ifdef __cplusplus
}
#endif

#endif
