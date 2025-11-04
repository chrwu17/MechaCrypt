// STM32L432KC_I2C_LCD.h
// Header for I2C LCD 20x4 functions

#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stm32l432xx.h>

///////////////////////////////////////////////////////////////////////////////
// Definitions
///////////////////////////////////////////////////////////////////////////////

// Default I2C address for PCF8574 I2C LCD adapter (usually 0x27 or 0x3F)
#define LCD_I2C_ADDR 0x3F

// LCD dimensions
#define LCD_COLS 20
#define LCD_ROWS 4

// LCD Commands
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x04
#define LCD_CMD_DISPLAY_CTRL    0x08
#define LCD_CMD_CURSOR_SHIFT    0x10
#define LCD_CMD_FUNCTION_SET    0x20
#define LCD_CMD_SET_CGRAM_ADDR  0x40
#define LCD_CMD_SET_DDRAM_ADDR  0x80

// Entry Mode flags
#define LCD_ENTRY_RIGHT         0x00
#define LCD_ENTRY_LEFT          0x02
#define LCD_ENTRY_SHIFT_INC     0x01
#define LCD_ENTRY_SHIFT_DEC     0x00

// Display Control flags
#define LCD_DISPLAY_ON          0x04
#define LCD_DISPLAY_OFF         0x00
#define LCD_CURSOR_ON           0x02
#define LCD_CURSOR_OFF          0x00
#define LCD_BLINK_ON            0x01
#define LCD_BLINK_OFF           0x00

// Function Set flags
#define LCD_8BIT_MODE           0x10
#define LCD_4BIT_MODE           0x00
#define LCD_2LINE               0x08
#define LCD_1LINE               0x00
#define LCD_5x10_DOTS           0x04
#define LCD_5x8_DOTS            0x00

// Backlight control
#define LCD_BACKLIGHT           0x08
#define LCD_NO_BACKLIGHT        0x00

// Enable bit
#define LCD_EN                  0x04
// Read/Write bit
#define LCD_RW                  0x02
// Register select bit
#define LCD_RS                  0x01

///////////////////////////////////////////////////////////////////////////////
// Function prototypes
///////////////////////////////////////////////////////////////////////////////

// Initialize I2C1 peripheral
void initI2C1(void);

// Low-level I2C LCD functions
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_send_string(char *str);

// High-level LCD functions
void lcd_init(void);
void lcd_clear(void);
void lcd_home(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_print(char *str);
void lcd_print_at(uint8_t row, uint8_t col, char *str);
void lcd_backlight_on(void);
void lcd_backlight_off(void);

// Progress bar functions
void lcd_create_progress_chars(void);
void lcd_show_progress(uint8_t row, uint8_t percent);

#endif