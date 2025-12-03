/**
 * @file mcu_to_mcu_spi_sender.c
 * @author Christian Wu
 * @date 2025-12-03
 * @brief Sender MCU - Uses shared SPI1 bus with separate CS pins
 */

#include "../lib/STM32L432KC.h"
#include "../lib/mcu_to_mcu_spi.h"

/**
 * @brief Initialize sender-side MCU-to-MCU communication
 * SPI1 already initialized for FPGA, just configure additional CS pin
 */
void initMCU_SPI_Sender(void) {
    // SPI1 is already initialized by initSPI() in main
    // Just configure the additional CS pin for receiver MCU
    pinMode(RX_MCU_CS, GPIO_OUTPUT);
    digitalWrite(RX_MCU_CS, 1);  // CS idle high (inactive)
}

/**
 * @brief Select receiver MCU on shared SPI bus
 */
static inline void selectReceiverMCU(void) {
    digitalWrite(RX_MCU_CS, 0);  // Assert CS for receiver MCU
}

/**
 * @brief Deselect receiver MCU on shared SPI bus
 */
static inline void deselectReceiverMCU(void) {
    digitalWrite(RX_MCU_CS, 1);  // Deassert CS for receiver MCU
}

/**
 * @brief Send all keys to receiver MCU using shared SPI1 bus
 * @param keys Array of 16-byte keys [MAX_BLOCKS][16]
 * @param num_blocks Number of blocks/keys to send
 * @param orig_length Original message length before padding
 * @return 0 on success, -1 on error
 * 
 * Uses hardware SPI1 (spiSendReceive) with separate CS pin
 */
int sendKeysToReceiverMCU(const uint8_t keys[][16], uint8_t num_blocks, uint8_t orig_length) {
    if (num_blocks == 0 || num_blocks > 64) {
        return -1;
    }
    
    // Assert CS for receiver MCU (FPGA CS stays high)
    selectReceiverMCU();
    delay_millis(TIM15, 5);  // Give receiver time to detect CS
    
    // Send header: [original_length][num_blocks]
    spiSendReceive(orig_length);
    spiSendReceive(num_blocks);
    
    delay_millis(TIM15, 5);  // Inter-packet delay
    
    // Send all keys sequentially using hardware SPI
    for (uint8_t block = 0; block < num_blocks; block++) {
        // Send 16-byte key via hardware SPI
        for (int byte_idx = 0; byte_idx < 16; byte_idx++) {
            spiSendReceive(keys[block][byte_idx]);
        }
        
        // Small delay between keys
        delay_millis(TIM15, 2);
        
        //// Optional: LED blink every 4 keys for feedback
        //if ((block % 4) == 0) {
        //    digitalWrite(LED_PIN, 1);
        //    delay_millis(TIM15, 50);
        //    digitalWrite(LED_PIN, 0);
        }
    
    
    // Deassert CS for receiver MCU
    delay_millis(TIM15, 5);
    deselectReceiverMCU();
    
    return 0;
}