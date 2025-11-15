// Example receiver main.c showing how to integrate SPI receive with webpage

#include "../lib/main.h"
#include "../lib/webpage.h"
#include "../lib/STM32L432KC.h"
#include "../lib/STM32L432KC_TIM.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_SPI.h"
#include "../lib/STM32L432KC_GPIO.h"

// Function prototype for storing received blocks (defined in webpage_receiver.c)
void store_received_block(int block_idx, const uint8_t *block16);

// SPI receive state machine
typedef enum {
  RX_IDLE = 0,
  RX_RECEIVING,
  RX_DONE_SIGNAL
} rx_state_t;

static volatile rx_state_t rx_state = RX_IDLE;
static volatile int current_rx_block = 0;
static uint8_t rx_buffer[16];

// Pins for FPGA handshake (adjust to your setup)
#define READY_PIN  PA5   // MCU -> FPGA: Ready to receive
#define VALID_PIN  PA6   // FPGA -> MCU: Data valid signal

static inline int valid_is_high(void) {
  return (digitalRead(VALID_PIN) != 0);
}

void init_spi_receiver(void) {
  // Configure handshake pins
  pinMode(READY_PIN, GPIO_OUTPUT);
  digitalWrite(READY_PIN, 1);  // Signal ready
  pinMode(VALID_PIN, GPIO_INPUT);
  
  // Initialize SPI (adjust BR, CPOL, CPHA as needed)
  initSPI(0b011, 0, 0);
  digitalWrite(SPI_CE, 1);  // CS idle high
  
  rx_state = RX_IDLE;
  current_rx_block = 0;
}

// Poll for incoming SPI data from FPGA
void poll_spi_receive(void) {
  if (rx_state == RX_IDLE) {
    // Check if FPGA signals data is valid
    if (valid_is_high()) {
      digitalWrite(READY_PIN, 0);  // Lower ready signal
      rx_state = RX_RECEIVING;
    }
  } else if (rx_state == RX_RECEIVING) {
    // Receive 16 bytes via SPI
    digitalWrite(SPI_CE, 0);
    for (int i = 0; i < 16; i++) {
      rx_buffer[i] = (uint8_t)spiSendReceive(0x00);  // Clock in data
    }
    digitalWrite(SPI_CE, 1);
    
    // Store the received block
    store_received_block(current_rx_block, rx_buffer);
    current_rx_block++;
    
    rx_state = RX_DONE_SIGNAL;
  } else if (rx_state == RX_DONE_SIGNAL) {
    // Wait for FPGA to lower VALID signal
    if (!valid_is_high()) {
      digitalWrite(READY_PIN, 1);  // Signal ready for next block
      rx_state = RX_IDLE;
    }
  }
}

int main(void) {
  // Core initialization
  configureFlash();
  configureClock();
  
  gpioEnable(GPIO_PORT_A);
  gpioEnable(GPIO_PORT_B);
  gpioEnable(GPIO_PORT_C);
  
  pinMode(LED_PIN, GPIO_OUTPUT);
  initTIM(TIM15);
  
  // Power-on blinks
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, 1);
    delay_millis(TIM15, 200);
    digitalWrite(LED_PIN, 0);
    delay_millis(TIM15, 200);
  }
  
  // Initialize USART for ESP8266
  USART_TypeDef *USART = initUSART(USART1_ID, 125000);
  
  // Initialize SPI receiver and handshake
  init_spi_receiver();
  
  // Main loop
  while (1) {
    processWebRequest(USART);   // Handle web requests
    poll_spi_receive();         // Check for incoming SPI data
  }
}