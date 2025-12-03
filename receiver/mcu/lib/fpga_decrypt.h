/**
 * @file fpga_decrypt.h
 * @author Christian Wu
 * @date 2025-11-25
 * @brief Header for FPGA AES decryption interface
 */

#ifndef FPGA_DECRYPT_H
#define FPGA_DECRYPT_H

#include <stdint.h>

/**
 * @brief Initialize FPGA decryption interface
 * Configures LOAD and DONE pins for communication with FPGA AES core
 */
void initFPGADecrypt(void);

/**
 * @brief Send ciphertext to FPGA and receive decrypted plaintext
 * @param ciphertext 16-byte input ciphertext block
 * @param plaintext 16-byte output plaintext block
 * @return 0 on success, -1 on timeout/error
 * 
 * Protocol (assumes key already loaded in FPGA via bridge):
 * 1. Assert LOAD
 * 2. Send 128 bits of ciphertext via SPI
 * 3. Deassert LOAD to start decryption
 * 4. Wait for DONE signal from FPGA
 * 5. Read 128 bits of plaintext via SPI
 */
int fpgaDecryptBlock(const uint8_t ciphertext[16], uint8_t plaintext[16]);

#endif // FPGA_DECRYPT_H