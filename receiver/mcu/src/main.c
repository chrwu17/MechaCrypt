#include "../lib/webpage.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/main.h"
#include <string.h>
#include <stdlib.h>

// ----------------- Test Data Setup -----------------
#define NUM_BLOCKS 3  // Simulating 3 blocks

// Simulated blocks (16-byte each)
uint8_t plaintext_blocks[NUM_BLOCKS][16] = {
  {'H', 'e', 'l', 'l', 'o', ' ', 'F', 'P', 'G', 'A', ' ', 'B', 'l', 'o', 'c', 'k'},
  {'T', 'e', 's', 't', ' ', 'B', 'l', 'o', 'c', 'k', ' ', 'T', 'w', 'o', ' ', 'D'},
  {'M', 'C', 'U', ' ', 'R', 'e', 'c', 'e', 'i', 'v', 'e', 'r', ' ', 'T', 'h', 'r', 'e'}
};

// Block status (which blocks are received)
uint8_t have_block[NUM_BLOCKS] = {1, 1, 1}; // All blocks are marked as received
volatile uint16_t total_blocks = NUM_BLOCKS;

// ----------------- Small ACK blink -----------------
static void led_blink_short(void) {
  digitalWrite(LED_PIN, 1);
  for (volatile int i = 0; i < 60000; ++i) __NOP();
  digitalWrite(LED_PIN, 0);
}

// ----------------- HTTP helpers -----------------
static const char http_header_ok[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_json[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nConnection: close\r\n\r\n";
static const char http_header_text[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\n";

// ----------------- Request handler for test case -----------------
void processWebRequest(USART_TypeDef *USART) {
  char request[BUFF_LEN] = {0};
  int idx = 0;

  // Read request line (up to LF)
  while (!(strchr(request, '\n'))) {
    while (!(USART->ISR & USART_ISR_RXNE)) { /*spin*/ }
    if (idx < (int)sizeof(request)-1) {
      request[idx++] = readChar(USART);
      request[idx] = '\0';
    } else {
      (void)readChar(USART); // discard extra
    }
  }

  // --- Serve the Receiver Page (GET /rx) ---
  if (strstr(request, "GET /rx ")) {
    sendString(USART, http_header_ok);
    sendString(USART, receiver_page);
    return;
  }

  // --- Provide Block Status (GET /status) ---
  if (strstr(request, "GET /status ")) {
    sendString(USART, http_header_json);
    sendString(USART, "{\"total\":");
    char tmp[16];
    sprintf(tmp, "%d", total_blocks);
    sendString(USART, tmp);
    sendString(USART, ",\"blocks\":{");

    int first = 1;
    for (uint16_t i = 0; i < total_blocks; i++) {
      if (have_block[i]) {
        if (!first) sendChar(USART, ',');
        first = 0;
        sendChar(USART, '\"'); sprintf(tmp, "%d", i); sendString(USART, tmp); sendChar(USART, '\"');
        sendChar(USART, ':');
        sendString(USART, "{\"hex\":\"");
        for (int k = 0; k < 16; k++) {
          if (k) sendChar(USART, ' ');
          send_hex_byte(USART, plaintext_blocks[i][k]);
        }
        sendString(USART, "\",\"bytes\":[");
        for (int k = 0; k < 16; k++) {
          if (k) sendChar(USART, ',');
          sprintf(tmp, "%d", plaintext_blocks[i][k]);
          sendString(USART, tmp);
        }
        sendString(USART, "]}");
      }
    }
    sendString(USART, "}}");
    return;
  }

  // --- Combine All Blocks into Message (GET /combine) ---
  if (strstr(request, "GET /combine ")) {
    uint32_t cap = (uint32_t)total_blocks * 16u;
    if (cap == 0u) { sendString(USART, http_header_text); sendString(USART, ""); return; }

    for (uint16_t i = 0; i < total_blocks; i++) {
      if (!have_block[i]) { sendString(USART, http_header_text); sendString(USART, ""); return; }
    }

    static uint8_t tmpbuf[NUM_BLOCKS * 16];
    uint32_t pos = 0;
    for (uint16_t i = 0; i < total_blocks; i++) {
      memcpy(&tmpbuf[pos], plaintext_blocks[i], 16);
      pos += 16;
    }

    int unpadded = pkcs7_unpad(tmpbuf, (int)pos);
    if (unpadded <= 0) unpadded = (int)pos;

    sendString(USART, http_header_text);
    for (int i = 0; i < unpadded; i++) {
      sendChar(USART, (char)tmpbuf[i]);
    }
    return;
  }
}
