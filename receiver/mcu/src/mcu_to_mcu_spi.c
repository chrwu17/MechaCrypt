/**
 * @file mcu_to_mcu_spi_receiver.c
 * @author Christian Wu
 * @date 2025-12-03
 * @brief Receiver MCU - Receives keys on shared SPI1 bus via separate CS
 */

#include "../lib/STM32L432KC.h"
#include "../lib/mcu_to_mcu_spi.h"
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// Storage for Received Keys
///////////////////////////////////////////////////////////////////////////////

static volatile uint8_t received_keys[64][16];      // Up to 64 blocks
static volatile uint8_t keys_received_flag = 0;     // 1 when complete
static volatile uint8_t num_blocks_expected = 0;
static volatile uint8_t original_msg_length = 0;
static volatile uint8_t num_keys_received = 0;

///////////////////////////////////////////////////////////////////////////////
// Reception State Machine
///////////////////////////////////////////////////////////////////////////////

typedef enum {
    KEY_RX_IDLE = 0,
    KEY_RX_HEADER,
    KEY_RX_KEYS,
    KEY_RX_COMPLETE
} key_rx_state_t;

static volatile key_rx_state_t rx_state = KEY_RX_IDLE;
static volatile uint8_t rx_byte_idx = 0;
static volatile uint8_t rx_block_idx = 0;
static volatile uint8_t header_buf[2];

///////////////////////////////////////////////////////////////////////////////
// CS Detection
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Check if CS is asserted (low) for receiver MCU
 */
static inline int isCSAsserted(void) {
    return (digitalRead(RX_MCU_CS_IN) == 0);
}

/**
 * @brief Initialize receiver-side MCU-to-MCU communication
 * SPI1 already initialized, configure CS input pin
 */
void initMCU_SPI_Receiver(void) {
    // SPI1 is already initialized by initSPI() in main
    // Configure CS input pin
    pinMode(RX_MCU_CS_IN, GPIO_INPUT);
    
    // Initialize state
    rx_state = KEY_RX_IDLE;
    rx_byte_idx = 0;
    rx_block_idx = 0;
    keys_received_flag = 0;
    num_blocks_expected = 0;
    num_keys_received = 0;
    
    memset((void*)received_keys, 0, sizeof(received_keys));
}

/**
 * @brief Poll for incoming key transmission on shared SPI bus
 * Call this repeatedly in main loop
 * 
 * Receiver must actively read SPI when its CS is asserted
 * 
 * @return 1 if reception in progress or complete, 0 if idle
 */
int pollKeyReception(void) {
    // Check if CS is asserted (active low)
    if (isCSAsserted()) {
        if (rx_state == KEY_RX_IDLE) {
            // Start of new transmission
            rx_state = KEY_RX_HEADER;
            rx_byte_idx = 0;
            rx_block_idx = 0;
        }
        
        // Process based on state using hardware SPI
        switch (rx_state) {
            case KEY_RX_HEADER:
                // Read 2-byte header
                if (rx_byte_idx < 2) {
                    // Read byte via hardware SPI (send dummy)
                    header_buf[rx_byte_idx] = spiSendReceive(0x00);
                    rx_byte_idx++;
                    
                    if (rx_byte_idx == 2) {
                        // Header complete
                        original_msg_length = header_buf[0];
                        num_blocks_expected = header_buf[1];
                        rx_state = KEY_RX_KEYS;
                        rx_byte_idx = 0;
                        rx_block_idx = 0;
                    }
                }
                break;
                
            case KEY_RX_KEYS:
                // Read 16-byte keys
                if (rx_block_idx < num_blocks_expected) {
                    // Read byte via hardware SPI
                    received_keys[rx_block_idx][rx_byte_idx] = spiSendReceive(0x00);
                    rx_byte_idx++;
                    
                    if (rx_byte_idx == 16) {
                        // Key complete
                        rx_byte_idx = 0;
                        rx_block_idx++;
                        num_keys_received = rx_block_idx;
                        
                        if (rx_block_idx >= num_blocks_expected) {
                            // All keys received
                            rx_state = KEY_RX_COMPLETE;
                            keys_received_flag = 1;
                        }
                    }
                }
                break;
                
            case KEY_RX_COMPLETE:
                // Wait for CS deassert
                break;
                
            default:
                break;
        }
        
        return 1;
    } else {
        // CS deasserted - reset if not complete
        if (rx_state != KEY_RX_COMPLETE) {
            rx_state = KEY_RX_IDLE;
        }
        return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Public Query Functions
///////////////////////////////////////////////////////////////////////////////

int areKeysReceived(void) {
    return keys_received_flag;
}

int getReceivedKey(uint8_t block_idx, uint8_t key_out[16]) {
    if (block_idx >= num_blocks_expected || !keys_received_flag) {
        return -1;
    }
    
    memcpy(key_out, (void*)received_keys[block_idx], 16);
    return 0;
}

uint8_t getNumKeysReceived(void) {
    return num_keys_received;
}

uint8_t getOriginalMessageLength(void) {
    return original_msg_length;
}

void resetKeyReception(void) {
    rx_state = KEY_RX_IDLE;
    rx_byte_idx = 0;
    rx_block_idx = 0;
    keys_received_flag = 0;
    num_blocks_expected = 0;
    num_keys_received = 0;
    original_msg_length = 0;
    memset((void*)received_keys, 0, sizeof(received_keys));
}