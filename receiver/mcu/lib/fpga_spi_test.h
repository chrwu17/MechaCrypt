/**
 * @file fpga_spi_test.h
 * @brief Header for FPGA SPI test functions
 * 
 * Add this to your receiver/mcu/lib/ directory
 */

#ifndef FPGA_SPI_TEST_H
#define FPGA_SPI_TEST_H

#define LED_PIN PA8
#define FPGA_LOAD PA5
#define FPGA_DONE PA6

/**
 * @brief Test FPGA decryption with a single known test vector
 * Decrypts one block and stores result for website display
 */
void test_fpga_decrypt_known_vector(void);

/**
 * @brief Test with multiple blocks (3 blocks = 48 bytes)
 * Creates a complete test message for website
 */
void test_fpga_decrypt_multi_block(void);

/**
 * @brief Loopback test (encrypt then decrypt)
 * Verifies full pipeline if encryption is available
 */
void test_fpga_loopback(void);

/**
 * @brief Fun ASCII art demo with 4 blocks
 * Quick visual test for website display
 */
void test_fpga_decrypt_ascii_art(void);

#endif // FPGA_SPI_TEST_H