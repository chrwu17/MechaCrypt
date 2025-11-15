#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>
#include <stddef.h>

/* === HD44780 Instruction/Flag Defines (same as Arduino lib) === */
#define LCD_CLEARDISPLAY   0x01
#define LCD_RETURNHOME     0x02
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT    0x10
#define LCD_FUNCTIONSET    0x20
#define LCD_SETCGRAMADDR   0x40
#define LCD_SETDDRAMADDR   0x80

/* Entry mode flags */
#define LCD_ENTRYRIGHT             0x00
#define LCD_ENTRYLEFT              0x02
#define LCD_ENTRYSHIFTINCREMENT    0x01
#define LCD_ENTRYSHIFTDECREMENT    0x00

/* Display control flags */
#define LCD_DISPLAYON   0x04
#define LCD_DISPLAYOFF  0x00
#define LCD_CURSORON    0x02
#define LCD_CURSOROFF   0x00
#define LCD_BLINKON     0x01
#define LCD_BLINKOFF    0x00

/* Cursor/Display shift flags */
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE  0x00
#define LCD_MOVERIGHT   0x04
#define LCD_MOVELEFT    0x00

/* Function set flags */
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE    0x08
#define LCD_1LINE    0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS  0x00

/* PCF8574 backlight */
#define LCD_BACKLIGHT    0x08
#define LCD_NOBACKLIGHT  0x00

/* PCF8574 pin mask (matches the common backpack):
   P7 P6 P5 P4 P3 P2 P1 P0
   D7 D6 D5 D4 BL EN RW RS
*/
#define LCD_EN 0x04
#define LCD_RW 0x02
#define LCD_RS 0x01

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl */
struct lcd_i2c;

/* === User-supplied low-level hooks ===
   Provide a blocking single-byte write to the PCF8574 at 'addr'.
   Returns 0 on success, non-zero on error.
*/
typedef int (*lcd_i2c_write_fn)(uint8_t addr7, uint8_t byte);

/* Optional microsecond delay hook (if NULL, a fallback is used) */
typedef void (*lcd_delay_us_fn)(uint32_t us);

/* Handle */
typedef struct lcd_i2c {
    uint8_t  addr;           /* 7-bit I2C address of PCF8574 (e.g., 0x27) */
    uint8_t  cols;
    uint8_t  rows;

    /* internal state mirrors Arduino lib */
    uint8_t  displayfunction;
    uint8_t  displaycontrol;
    uint8_t  displaymode;
    uint8_t  numlines;
    uint8_t  backlight;      /* LCD_BACKLIGHT or LCD_NOBACKLIGHT */

    /* hooks */
    lcd_i2c_write_fn write_byte;  /* REQUIRED */
    lcd_delay_us_fn  delay_us;    /* OPTIONAL */
} lcd_i2c_t;

/* === API (Arduino-like) === */
void lcd_init(lcd_i2c_t *lcd, uint8_t addr7, uint8_t cols, uint8_t rows,
              lcd_i2c_write_fn write_cb, lcd_delay_us_fn delay_cb);

void lcd_begin(lcd_i2c_t *lcd);           /* run the init sequence */
void lcd_clear(lcd_i2c_t *lcd);
void lcd_home(lcd_i2c_t *lcd);
void lcd_set_cursor(lcd_i2c_t *lcd, uint8_t col, uint8_t row);

void lcd_display(lcd_i2c_t *lcd);
void lcd_no_display(lcd_i2c_t *lcd);
void lcd_cursor(lcd_i2c_t *lcd);
void lcd_no_cursor(lcd_i2c_t *lcd);
void lcd_blink(lcd_i2c_t *lcd);
void lcd_no_blink(lcd_i2c_t *lcd);

void lcd_scroll_display_left(lcd_i2c_t *lcd);
void lcd_scroll_display_right(lcd_i2c_t *lcd);
void lcd_left_to_right(lcd_i2c_t *lcd);
void lcd_right_to_left(lcd_i2c_t *lcd);
void lcd_autoscroll(lcd_i2c_t *lcd);
void lcd_no_autoscroll(lcd_i2c_t *lcd);

void lcd_create_char(lcd_i2c_t *lcd, uint8_t location, const uint8_t charmap[8]);

void lcd_backlight(lcd_i2c_t *lcd);
void lcd_no_backlight(lcd_i2c_t *lcd);

/* Writing data/commands */
void lcd_command(lcd_i2c_t *lcd, uint8_t value);
void lcd_write(lcd_i2c_t *lcd, uint8_t value);          /* write one character */
void lcd_print(lcd_i2c_t *lcd, const char *s);          /* write C-string */
void lcd_print_n(lcd_i2c_t *lcd, const char *s, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LCD_I2C_H */
