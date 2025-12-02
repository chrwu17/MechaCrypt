/**
 * @file fpga_decrypt.h
 * @author Christian Wu
 * @date 2025-11-25
 * @brief Header for FPGA AES decryption interface
 *        (Key is already in FPGA from bridge module)
 */

#ifndef FPGA_DECRYPT_H
#define FPGA_DECRYPT_H

#include <stdint.h>

/**
 * @brief Initialize FPGA decryption interface
 *        Sets up control pins (LOAD, DONE)
 *        Note: Key is already loaded in FPGA from bridge
 */
void initFPGADecrypt(void);

/**
 * @brief Decrypt a 16-byte ciphertext block using FPGA
 * @param ciphertext 16-byte input ciphertext
 * @param plaintext 16-byte output plaintext
 * @return 0 on success, -1 on timeout/error
 * 
 * The FPGA already has the decryption key from the bridge module.
 * This function only sends the ciphertext and receives plaintext.
 */
int fpgaDecryptBlock(const uint8_t ciphertext[16], uint8_t plaintext[16]);

#endif // FPGA_DECRYPT_H