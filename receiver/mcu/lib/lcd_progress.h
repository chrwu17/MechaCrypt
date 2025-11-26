/**
 * @file lcd_progress.h
 * @author Christian Wu
 * @date 2025-11-25
 * @brief Header for LCD progress bar display functions
 */

#ifndef LCD_PROGRESS_H
#define LCD_PROGRESS_H

#include <stdint.h>
#include "lcd_i2c.h"

// External variables tracking progress
extern volatile uint16_t received_block_count;
extern volatile uint16_t total_expected_blocks;

/**
 * @brief Initialize custom characters for progress bar
 * @param lcd Pointer to LCD handle
 * 
 * Call this once after lcd_begin() to create the custom
 * progress bar characters in LCD memory.
 */
void lcd_init_progress_chars(lcd_i2c_t *lcd);

/**
 * @brief Display progress bar on LCD
 * @param lcd Pointer to LCD handle
 * @param row Row to display progress bar (0-3 for 20x4 LCD)
 * @param received Number of blocks received
 * @param total Total number of blocks expected
 * @param bar_width Width of progress bar in characters (recommend 16-18)
 * 
 * Draws a smooth progress bar using custom characters.
 */
void lcd_show_progress(lcd_i2c_t *lcd, uint8_t row, uint16_t received, 
                       uint16_t total, uint8_t bar_width);

/**
 * @brief Update LCD display with transfer status
 * @param lcd Pointer to LCD handle
 * @param received Number of blocks received
 * @param total Total blocks expected
 * 
 * Updates all four lines of the LCD:
 *   Line 0: Title
 *   Line 1: Status text with counts
 *   Line 2: Progress bar with percentage
 *   Line 3: Bytes transferred
 */
void lcd_update_transfer_status(lcd_i2c_t *lcd, uint16_t received, uint16_t total);

/**
 * @brief Set the expected transfer size
 * @param msg_length_bytes Total message length in bytes
 * 
 * Call this when you receive the message length from the bridge.
 * Calculates the number of 16-byte blocks and resets the counter.
 */
void set_expected_transfer_size(uint16_t msg_length_bytes);

/**
 * @brief Increment block counter and update display
 * @param lcd Pointer to LCD handle
 * 
 * Call this whenever a complete 16-byte block is received
 * via the mechanical system.
 */
void on_block_received(lcd_i2c_t *lcd);

/**
 * @brief Get current received block count
 * @return Number of blocks received so far
 */
uint16_t get_received_block_count(void);

/**
 * @brief Get total expected blocks
 * @return Total number of blocks expected
 */
uint16_t get_total_expected_blocks(void);

/**
 * @brief Reset progress counters to zero
 */
void reset_progress(void);

#endif // LCD_PROGRESS_H