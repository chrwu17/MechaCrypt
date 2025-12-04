/**
 * @file fpga_spi_test_debug.c
 * @brief Debug version with better timing and diagnostics
 */

#include "../lib/STM32L432KC.h"
#include "../lib/fpga_decrypt.h"
#include "../lib/webpage.h"
#include "../lib/lcd_i2c.h"
#include "../lib/fpga_spi_test.h"
#include <string.h>

// External LCD handle (from main.c)
extern lcd_i2c_t g_lcd;

/**
 * @brief Display hex byte on LCD for debugging
 */
void lcd_print_hex_byte(lcd_i2c_t *lcd, uint8_t byte) {
    char buf[4];
    const char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(byte >> 4) & 0xF];
    buf[1] = hex[byte & 0xF];
    buf[2] = ' ';
    buf[3] = '\0';
    lcd_print(lcd, buf);
}

/**
 * @brief Test basic SPI communication (loopback check)
 */
int test_spi_basic(void) {
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "SPI Basic Test      ");
    
    // Test pattern
    uint8_t test_patterns[] = {0x00, 0xFF, 0xAA, 0x55};
    uint8_t received;
    int failures = 0;
    
    digitalWrite(SPI_CE, 0);  // Select FPGA
    delay_millis(TIM15, 10);   // Long delay for setup
    
    for (int i = 0; i < 4; i++) {
        received = spiSendReceive(test_patterns[i]);
        
        lcd_set_cursor(&g_lcd, 0, 1);
        lcd_print(&g_lcd, "Sent: ");
        lcd_print_hex_byte(&g_lcd, test_patterns[i]);
        lcd_print(&g_lcd, "Recv: ");
        lcd_print_hex_byte(&g_lcd, received);
        
        delay_millis(TIM15, 500);
    }
    
    digitalWrite(SPI_CE, 1);  // Deselect
    delay_millis(TIM15, 10);
    
    return failures;
}

/**
 * @brief Check if FPGA DONE signal responds
 */
int test_fpga_done_signal(void) {
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Testing DONE Signal ");
    
    // Test without load
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "LOAD=0: ");
    digitalWrite(FPGA_LOAD, 0);
    delay_millis(TIM15, 100);
    int done_low = digitalRead(FPGA_DONE);
    lcd_print(&g_lcd, done_low ? "HIGH" : "LOW ");
    
    delay_millis(TIM15, 1000);
    
    // Test with load
    lcd_set_cursor(&g_lcd, 0, 2);
    lcd_print(&g_lcd, "LOAD=1: ");
    digitalWrite(FPGA_LOAD, 1);
    delay_millis(TIM15, 100);
    int done_high = digitalRead(FPGA_DONE);
    lcd_print(&g_lcd, done_high ? "HIGH" : "LOW ");
    
    digitalWrite(FPGA_LOAD, 0);
    delay_millis(TIM15, 2000);
    
    return (done_low == 0 && done_high == 0) ? 0 : -1;
}

/**
 * @brief Improved FPGA decrypt with better timing and error reporting
 */
int fpgaDecryptBlock_Debug(const uint8_t key[16], const uint8_t ciphertext[16], 
                           uint8_t plaintext[16], lcd_i2c_t *lcd) {
    
    lcd_clear(lcd);
    lcd_set_cursor(lcd, 0, 0);
    lcd_print(lcd, "Decrypt Starting... ");
    
    // Step 1: Assert LOAD
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "1. Assert LOAD      ");
    digitalWrite(FPGA_LOAD, 1);
    delay_millis(TIM15, 50);  // Increased delay
    
    // Step 2: Select FPGA
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "2. CS Low (Select)  ");
    digitalWrite(SPI_CE, 0);
    delay_millis(TIM15, 10);
    
    // Step 3: Send KEY (MSB first)
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "3. Sending Key...   ");
    for (int i = 15; i >= 0; i--) {
        spiSendReceive(key[i]);
        // Small delay between bytes
        for (volatile int j = 0; j < 100; j++) { __NOP(); }
    }
    delay_millis(TIM15, 10);
    
    // Step 4: Send CIPHERTEXT (MSB first)
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "4. Sending Cipher...");
    for (int i = 15; i >= 0; i--) {
        spiSendReceive(ciphertext[i]);
        for (volatile int j = 0; j < 100; j++) { __NOP(); }
    }
    delay_millis(TIM15, 10);
    
    // Step 5: Deselect FPGA
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "5. CS High          ");
    digitalWrite(SPI_CE, 1);
    delay_millis(TIM15, 10);
    
    // Step 6: Deassert LOAD to start decryption
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "6. LOAD Low (Start) ");
    digitalWrite(FPGA_LOAD, 0);
    delay_millis(TIM15, 50);
    
    // Step 7: Wait for DONE with timeout
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "7. Waiting DONE...  ");
    
    uint32_t timeout = 500000;  // Much longer timeout
    uint32_t start_time = get_millis();
    
    while (!digitalRead(FPGA_DONE) && timeout > 0) {
        timeout--;
        // Update LCD every 100ms
        if ((get_millis() - start_time) % 100 == 0) {
            lcd_set_cursor(lcd, 0, 2);
            char buf[21];
            snprintf(buf, sizeof(buf), "Timeout: %lu     ", 500000 - timeout);
            lcd_print(lcd, buf);
        }
    }
    
    if (timeout == 0) {
        lcd_set_cursor(lcd, 0, 1);
        lcd_print(lcd, "ERROR: Timeout!     ");
        lcd_set_cursor(lcd, 0, 2);
        lcd_print(lcd, "DONE never HIGH     ");
        delay_millis(TIM15, 3000);
        return -1;
    }
    
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "8. DONE! Reading... ");
    delay_millis(TIM15, 10);
    
    // Step 8: Select FPGA again to read result
    digitalWrite(SPI_CE, 0);
    delay_millis(TIM15, 10);
    
    // Step 9: Read plaintext (MSB first)
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "9. Reading output...");
    for (int i = 15; i >= 0; i--) {
        plaintext[i] = spiSendReceive(0x00);
        for (volatile int j = 0; j < 100; j++) { __NOP(); }
    }
    
    // Step 10: Deselect
    digitalWrite(SPI_CE, 1);
    
    lcd_set_cursor(lcd, 0, 1);
    lcd_print(lcd, "SUCCESS!            ");
    
    // Display first 8 bytes
    lcd_set_cursor(lcd, 0, 2);
    for (int i = 0; i < 8; i++) {
        lcd_print_hex_byte(lcd, plaintext[i]);
    }
    
    delay_millis(TIM15, 2000);
    
    return 0;
}

/**
 * @brief Test with ALL ZEROS (simplest test)
 */
void test_all_zeros(void) {
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Test: All Zeros     ");
    
    uint8_t key[16] = {0};
    uint8_t ciphertext[16] = {0};
    uint8_t plaintext[16];
    
    delay_millis(TIM15, 1000);
    
    int result = fpgaDecryptBlock_Debug(key, ciphertext, plaintext, &g_lcd);
    
    if (result == 0) {
        // Store result
        receiver_store_block(0, plaintext);
        
        // Success blinks
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, 1);
            delay_millis(TIM15, 200);
            digitalWrite(LED_PIN, 0);
            delay_millis(TIM15, 200);
        }
    } else {
        // Error blinks
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, 1);
            delay_millis(TIM15, 50);
            digitalWrite(LED_PIN, 0);
            delay_millis(TIM15, 50);
        }
    }
}

/**
 * @brief Test with known AES test vector
 */
void test_known_vector(void) {
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Test: Known Vector  ");
    
    // Standard NIST test vector
    uint8_t key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };
    
    // Ciphertext for plaintext "0123456789abcdef" (or use all zeros for now)
    uint8_t ciphertext[16] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    uint8_t plaintext[16];
    
    delay_millis(TIM15, 1000);
    
    int result = fpgaDecryptBlock_Debug(key, ciphertext, plaintext, &g_lcd);
    
    if (result == 0) {
        receiver_store_block(0, plaintext);
        
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, 1);
            delay_millis(TIM15, 200);
            digitalWrite(LED_PIN, 0);
            delay_millis(TIM15, 200);
        }
    }
}

/**
 * @brief Run comprehensive diagnostics
 */
void run_fpga_diagnostics(void) {
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "FPGA Diagnostics    ");
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Starting tests...   ");
    delay_millis(TIM15, 2000);
    
    // Test 1: SPI Basic
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Test 1/3: SPI Basic ");
    delay_millis(TIM15, 1000);
    test_spi_basic();
    
    // Test 2: DONE Signal
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Test 2/3: DONE Pin  ");
    delay_millis(TIM15, 1000);
    test_fpga_done_signal();
    
    // Test 3: Full Decrypt
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Test 3/3: Decrypt   ");
    delay_millis(TIM15, 1000);
    test_all_zeros();
    
    // Summary
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Diagnostics Done    ");
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Check LCD messages  ");
    lcd_set_cursor(&g_lcd, 0, 2);
    lcd_print(&g_lcd, "Press button again  ");
}

/**
 * @brief Simple test - just try to get ANY response
 */
void test_minimal(void) {
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Minimal Test        ");
    
    // Just store some test data directly (bypass FPGA for now)
    uint8_t test_data[16] = "MINIMAL TEST OK!";
    receiver_store_block(0, test_data);
    
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Data stored         ");
    lcd_set_cursor(&g_lcd, 0, 2);
    lcd_print(&g_lcd, "Check website!      ");
    
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, 1);
        delay_millis(TIM15, 100);
        digitalWrite(LED_PIN, 0);
        delay_millis(TIM15, 100);
    }
}