/**
 * @file fpga_decrypt.h
 * @author Christian Wu
 * @date 2025-12-03
 * @brief Updated header for FPGA AES decryption with key+ciphertext input
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
 * @brief Send key and ciphertext to FPGA, receive decrypted plaintext
 * @param key 16-byte decryption key
 * @param ciphertext 16-byte input ciphertext block
 * @param plaintext 16-byte output plaintext block
 * @return 0 on success, -1 on timeout/error
 * 
 * NEW Protocol (matches sender encryption):
 * 1. Assert LOAD
 * 2. Send 128 bits KEY via SPI
 * 3. Send 128 bits CIPHERTEXT via SPI
 * 4. Deassert LOAD to start decryption
 * 5. Wait for DONE signal from FPGA
 * 6. Read 128 bits PLAINTEXT via SPI
 */
int fpgaDecryptBlock(const uint8_t key[16], const uint8_t ciphertext[16], uint8_t plaintext[16]);

/**
 * @brief Decrypt block using key fetched via callback
 * @param block_idx Block index for key lookup
 * @param ciphertext 16-byte ciphertext input
 * @param plaintext 16-byte plaintext output
 * @param getKeyFunc Callback to fetch key: int getKey(uint8_t idx, uint8_t key[16])
 * @return 0 on success, -1 on error
 */
int fpgaDecryptBlockWithIndex(uint8_t block_idx, 
                               const uint8_t ciphertext[16], 
                               uint8_t plaintext[16],
                               int (*getKeyFunc)(uint8_t, uint8_t*));

#endif // FPGA_DECRYPT_H