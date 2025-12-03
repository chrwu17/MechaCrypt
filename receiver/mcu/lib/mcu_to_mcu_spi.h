/**
 * @file mcu_to_mcu_spi.h
 * @author Christian Wu
 * @date 2025-12-03
 * @brief Header for MCU-to-MCU SPI key transfer (Sender -> Receiver)
 * 
 * Protocol:
 * 1. Sender sends header: [msg_length_byte][num_blocks_byte]
 * 2. Sender sends N keys: [16 bytes per key] x N blocks
 * 3. Receiver stores keys for later use during decryption
 */

#ifndef MCU_TO_MCU_SPI_H
#define MCU_TO_MCU_SPI_H

#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// Pin Definitions - Shared SPI Bus
///////////////////////////////////////////////////////////////////////////////

// SHARED SPI1 signals (same bus as FPGA)
#define SHARED_SPI_SCK    PB3   // Shared SPI clock
#define SHARED_SPI_COPI   PB5   // Shared SPI MOSI
#define SHARED_SPI_CIPO   PB4   // Shared SPI MISO

// SENDER: Separate chip selects
#define FPGA_CS           PA11  // CS for FPGA (encryption)
#define RX_MCU_CS         PA8   // CS for receiver MCU (key transfer)

// RECEIVER: Separate chip selects  
#define RX_FPGA_CS        PA11  // CS for FPGA (decryption)
#define RX_MCU_CS_IN      PA8   // CS input from sender MCU

///////////////////////////////////////////////////////////////////////////////
// Protocol Constants
///////////////////////////////////////////////////////////////////////////////

#define MCU_SPI_HEADER_SIZE  2    // [length][blocks]
#define MCU_SPI_KEY_SIZE     16   // 128-bit keys

///////////////////////////////////////////////////////////////////////////////
// Sender Functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Initialize shared SPI bus (sender side)
 * SPI1 already initialized for FPGA, just configure additional CS pin
 */
void initMCU_SPI_Sender(void);

/**
 * @brief Send all keys to receiver MCU using shared SPI1 bus
 * @param keys Array of 16-byte keys
 * @param num_blocks Number of blocks/keys to send
 * @param orig_length Original message length before padding
 * @return 0 on success, -1 on error
 * 
 * Protocol (uses hardware SPI1):
 * 1. Assert RX_MCU_CS (PA8 low)
 * 2. Send header: [orig_length][num_blocks] via spiSendReceive()
 * 3. Send each 16-byte key via spiSendReceive()
 * 4. Deassert RX_MCU_CS (PA8 high)
 */
int sendKeysToReceiverMCU(const uint8_t keys[][16], uint8_t num_blocks, uint8_t orig_length);

///////////////////////////////////////////////////////////////////////////////
// Receiver Functions
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Initialize receiver SPI pins for key reception
 * Configures CS input pin and enables interrupt on CS falling edge
 * SPI1 peripheral already initialized, shared with FPGA
 */
void initMCU_SPI_Receiver(void);

/**
 * @brief Check if key reception is complete
 * @return 1 if all keys received, 0 otherwise
 */
int areKeysReceived(void);

/**
 * @brief Poll for incoming keys (receiver must actively read when CS asserted)
 * Call this in main loop to check for incoming key transmission
 * @return 1 if reception in progress or complete, 0 if idle
 */
int pollKeyReception(void);

/**
 * @brief Get received key for a specific block
 * @param block_idx Block index (0 to num_blocks-1)
 * @param key_out Output buffer for 16-byte key
 * @return 0 on success, -1 if key not available
 */
int getReceivedKey(uint8_t block_idx, uint8_t key_out[16]);

/**
 * @brief Get total number of keys received
 * @return Number of blocks expected
 */
uint8_t getNumKeysReceived(void);

/**
 * @brief Get original message length
 * @return Original length before padding
 */
uint8_t getOriginalMessageLength(void);

/**
 * @brief Reset key reception state
 */
void resetKeyReception(void);

#endif // MCU_TO_MCU_SPI_H