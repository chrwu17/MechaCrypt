/**
 * @file fpga_decrypt.c
 * @author Christian Wu
 * @date 2025-11-25
 * @brief SPI interface to FPGA AES decryption module
 *        Sends 128-bit ciphertext, receives 128-bit plaintext
 *        (FPGA already has the key from bridge module)
 */

#include "../lib/STM32L432KC.h"
#include "../lib/fpga_decrypt.h"


// FPGA control pins
#define FPGA_LOAD  PA5   // Load signal to start decryption
#define FPGA_DONE  PA6  // Done signal from FPGA

/**
 * @brief Initialize FPGA decryption interface
 */
void initFPGADecrypt(void) {
    // Configure control pins
    pinMode(FPGA_LOAD, GPIO_OUTPUT);
    pinMode(FPGA_DONE, GPIO_INPUT);
    
    // Initialize load signal low
    digitalWrite(FPGA_LOAD, 0);
    
    // SPI should already be initialized by main.c
}

/**
 * @brief Send ciphertext to FPGA and receive decrypted plaintext
 * @param ciphertext 16-byte input ciphertext block
 * @param plaintext 16-byte output plaintext block
 * @return 0 on success, -1 on timeout
 * 
 * Protocol (simplified - FPGA already has key):
 * 1. Assert LOAD
 * 2. Send 128 bits via SPI: ciphertext[127:0]
 * 3. Deassert LOAD
 * 4. Wait for DONE signal
 * 5. Send 128 dummy bits to clock out plaintext[127:0]
 */
int fpgaDecryptBlock(const uint8_t ciphertext[16], uint8_t plaintext[16]) {
    
    // Step 1: Assert LOAD signal
    digitalWrite(FPGA_LOAD, 1);
    
    // Small delay to ensure LOAD is recognized
    delay_millis(TIM15, 1);
    
    // Step 2: Select FPGA (CS low)
    digitalWrite(SPI_CE, 0);
    
    // Send 128 bits: CIPHERTEXT (MSB first)
    // Send ciphertext[15] down to ciphertext[0]
    for (int i = 15; i >= 0; i--) {
        spiSendReceive(ciphertext[i]);
    }
    
    // Deselect FPGA (CS high)
    digitalWrite(SPI_CE, 1);
    
    // Step 3: Deassert LOAD signal to start decryption
    digitalWrite(FPGA_LOAD, 0);
    
    // Step 4: Wait for DONE signal (with timeout)
    uint32_t timeout = 1000000;  // Adjust based on your clock speed
    while (!digitalRead(FPGA_DONE) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        // Timeout - decryption didn't complete
        return -1;
    }
    
    // Step 5: DONE is asserted, now clock out plaintext
    // Select FPGA again
    digitalWrite(SPI_CE, 0);
    
    // Clock out 128 bits of plaintext (MSB first)
    // FPGA shifts out plaintext[127] down to plaintext[0]
    for (int i = 15; i >= 0; i--) {
        plaintext[i] = (uint8_t)spiSendReceive(0x00);  // Send dummy, receive data
    }
    
    // Deselect FPGA
    digitalWrite(SPI_CE, 1);
    
    return 0;  // Success
}