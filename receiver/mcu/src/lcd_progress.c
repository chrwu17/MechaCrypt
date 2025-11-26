/**
 * @file lcd_progress.c
 * @author Christian Wu
 * @date 2025-11-25
 * @brief LCD progress bar display for MechaCrypt receiver
 */

#include "../lib/lcd_progress.h"
#include <stdio.h>

// Global state
volatile uint16_t received_block_count = 0;
volatile uint16_t total_expected_blocks = 0;

/**
 * @brief Create a custom progress bar character for LCD
 * @param lcd Pointer to LCD handle
 * @param location Character location (0-7)
 * @param filled Number of pixels to fill (0-5)
 */
static void create_progress_char(lcd_i2c_t *lcd, uint8_t location, uint8_t filled) {
    uint8_t charmap[8];
    
    // Create horizontal bar pattern
    for (int i = 0; i < 8; i++) {
        if (filled == 0) {
            charmap[i] = 0b00000;  // Empty
        } else if (filled == 1) {
            charmap[i] = 0b10000;  // 1/5 filled
        } else if (filled == 2) {
            charmap[i] = 0b11000;  // 2/5 filled
        } else if (filled == 3) {
            charmap[i] = 0b11100;  // 3/5 filled
        } else if (filled == 4) {
            charmap[i] = 0b11110;  // 4/5 filled
        } else {
            charmap[i] = 0b11111;  // Fully filled
        }
    }
    
    lcd_create_char(lcd, location, charmap);
}

/**
 * @brief Initialize custom characters for progress bar
 * @param lcd Pointer to LCD handle
 */
void lcd_init_progress_chars(lcd_i2c_t *lcd) {
    create_progress_char(lcd, 0, 0);  // Empty
    create_progress_char(lcd, 1, 1);  // 1/5
    create_progress_char(lcd, 2, 2);  // 2/5
    create_progress_char(lcd, 3, 3);  // 3/5
    create_progress_char(lcd, 4, 4);  // 4/5
    create_progress_char(lcd, 5, 5);  // Full
}

/**
 * @brief Display progress bar on LCD
 * @param lcd Pointer to LCD handle
 * @param row Row to display progress bar (0-3 for 20x4 LCD)
 * @param received Number of blocks received
 * @param total Total number of blocks expected
 * @param bar_width Width of progress bar in characters (recommend 16-18)
 */
void lcd_show_progress(lcd_i2c_t *lcd, uint8_t row, uint16_t received, uint16_t total, uint8_t bar_width) {
    if (total == 0) return;  // Avoid division by zero
    
    // Calculate percentage
    uint16_t percent = (received * 100) / total;
    if (percent > 100) percent = 100;
    
    // Calculate progress bar segments
    // Each character position can show 5 levels of fill
    uint32_t filled_pixels = ((uint32_t)received * bar_width * 5) / total;
    uint8_t full_chars = filled_pixels / 5;
    uint8_t partial_fill = filled_pixels % 5;
    
    // Set cursor to start of row
    lcd_set_cursor(lcd, 0, row);
    
    // Draw full characters
    for (uint8_t i = 0; i < full_chars && i < bar_width; i++) {
        lcd_write(lcd, 5);  // Full block custom char
    }
    
    // Draw partial character
    if (full_chars < bar_width && partial_fill > 0) {
        lcd_write(lcd, partial_fill);  // Partial fill custom char
        full_chars++;
    }
    
    // Draw empty characters
    for (uint8_t i = full_chars; i < bar_width; i++) {
        lcd_write(lcd, 0);  // Empty custom char
    }
    
    // Display percentage if there's room
    if (bar_width < 17) {
        char pct_str[8];
        snprintf(pct_str, sizeof(pct_str), " %3u%%", percent);
        lcd_print(lcd, pct_str);
    }
}

/**
 * @brief Simple helper to convert uint16 to string
 */
static void uint16_to_str(uint16_t num, char *str, uint8_t min_width) {
    char temp[6];
    int i = 0;
    
    if (num == 0) {
        temp[i++] = '0';
    } else {
        while (num > 0) {
            temp[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    
    // Add padding
    int j = 0;
    while (j < (min_width - i)) {
        str[j++] = ' ';
    }
    
    // Reverse digits
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

/**
 * @brief Update LCD display with transfer status
 * @param lcd Pointer to LCD handle
 * @param received Number of blocks received
 * @param total Total blocks expected
 */
void lcd_update_transfer_status(lcd_i2c_t *lcd, uint16_t received, uint16_t total) {
    char num_str[8];
    
    // Line 0: Title
    lcd_set_cursor(lcd, 0, 0);
    lcd_print(lcd, "MechaCrypt Receiver ");
    
    // Line 1: Status text with percentage
    lcd_set_cursor(lcd, 0, 1);
    if (total == 0) {
        lcd_print(lcd, "Waiting for data... ");
    } else if (received >= total) {
        lcd_print(lcd, "Complete!      100% ");
    } else {
        lcd_print(lcd, "RX: ");
        uint16_to_str(received, num_str, 2);
        lcd_print(lcd, num_str);
        lcd_print(lcd, "/");
        uint16_to_str(total, num_str, 2);
        lcd_print(lcd, num_str);
        
        // Add percentage on same line
        uint16_t pct = (received * 100) / total;
        if (pct > 100) pct = 100;
        lcd_print(lcd, "  ");
        uint16_to_str(pct, num_str, 3);
        lcd_print(lcd, num_str);
        lcd_print(lcd, "%     ");
    }
    
    // Line 2: Full-width progress bar (20 characters)
    lcd_set_cursor(lcd, 0, 2);
    if (total > 0) {
        lcd_show_progress(lcd, 2, received, total, 20);
    } else {
        lcd_print(lcd, "                    ");
    }
    
    // Line 3: Bytes transferred
    lcd_set_cursor(lcd, 0, 3);
    if (total > 0) {
        uint32_t bytes_recv = (uint32_t)received * 16;
        uint32_t bytes_tot = (uint32_t)total * 16;
        
        // Show received bytes
        uint16_to_str((uint16_t)bytes_recv, num_str, 1);
        lcd_print(lcd, num_str);
        lcd_print(lcd, "/");
        uint16_to_str((uint16_t)bytes_tot, num_str, 1);
        lcd_print(lcd, num_str);
        lcd_print(lcd, " bytes      ");
    } else {
        lcd_print(lcd, "                    ");
    }
}

/**
 * @brief Call this when you receive the message length from bridge
 * @param msg_length_bytes Total message length in bytes
 */
void set_expected_transfer_size(uint16_t msg_length_bytes) {
    // Calculate number of 128-bit (16-byte) blocks needed
    total_expected_blocks = (msg_length_bytes + 15) / 16;  // Round up
    
    // Reset counter
    received_block_count = 0;
}

/**
 * @brief Call this whenever a new block is received via mechanical system
 * @param lcd Pointer to LCD handle
 */
void on_block_received(lcd_i2c_t *lcd) {
    received_block_count++;
    
    // Update LCD display
    lcd_update_transfer_status(lcd, received_block_count, total_expected_blocks);
}

/**
 * @brief Get current received block count
 * @return Number of blocks received
 */
uint16_t get_received_block_count(void) {
    return received_block_count;
}

/**
 * @brief Get total expected blocks
 * @return Total blocks expected
 */
uint16_t get_total_expected_blocks(void) {
    return total_expected_blocks;
}

/**
 * @brief Reset progress counters
 */
void reset_progress(void) {
    received_block_count = 0;
    total_expected_blocks = 0;
}