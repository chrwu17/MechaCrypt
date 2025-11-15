#include "../lib/lcd_i2c.h"

// pulse enable, matching your old working timing
static void pulse_enable(lcd_i2c_t *lcd, uint8_t val)
{
    lcd->write_byte(lcd->addr, val | LCD_EN);
    if (lcd->delay_us) lcd->delay_us(10); // working value before cleanup

    lcd->write_byte(lcd->addr, val & ~LCD_EN);
    if (lcd->delay_us) lcd->delay_us(50);
}

static void write4bits(lcd_i2c_t *lcd, uint8_t data)
{
    uint8_t out = data | lcd->backlight;
    lcd->write_byte(lcd->addr, out);
    pulse_enable(lcd, out);
}

void lcd_command(lcd_i2c_t *lcd, uint8_t v)
{
    write4bits(lcd, v & 0xF0);
    write4bits(lcd, (v << 4) & 0xF0);
}

void lcd_write(lcd_i2c_t *lcd, uint8_t v)
{
    write4bits(lcd, (v & 0xF0) | LCD_RS);
    write4bits(lcd, ((v << 4) & 0xF0) | LCD_RS);
}

void lcd_init(lcd_i2c_t *lcd, uint8_t addr7, uint8_t cols, uint8_t rows,
              lcd_i2c_write_fn write_cb, lcd_delay_us_fn delay_cb)
{
    lcd->addr = addr7;
    lcd->cols = cols;
    lcd->rows = rows;

    lcd->write_byte = write_cb;
    lcd->delay_us = delay_cb;

    lcd->backlight = LCD_BACKLIGHT;

    lcd->displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
    lcd->displaycontrol  = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    lcd->displaymode     = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
}

void lcd_begin(lcd_i2c_t *lcd)
{
    lcd->delay_us(50000);

    lcd_command(lcd, 0x33);
    lcd_command(lcd, 0x32);
    lcd_command(lcd, 0x28);
    lcd_command(lcd, 0x0C);
    lcd_command(lcd, 0x06);
    lcd_clear(lcd);
}

void lcd_clear(lcd_i2c_t *lcd)
{
    lcd_command(lcd, LCD_CLEARDISPLAY);
    lcd->delay_us(2000);
}

void lcd_home(lcd_i2c_t *lcd)
{
    lcd_command(lcd, LCD_RETURNHOME);
    lcd->delay_us(2000);
}

void lcd_set_cursor(lcd_i2c_t *lcd, uint8_t col, uint8_t row)
{
    static const uint8_t offsets[] = {0, 64, 20, 84};
    lcd_command(lcd, LCD_SETDDRAMADDR | (col + offsets[row]));
}

void lcd_print(lcd_i2c_t *lcd, const char *s)
{
    while (*s) lcd_write(lcd, *s++);
}

void lcd_backlight(lcd_i2c_t *lcd)
{
    lcd->backlight = LCD_BACKLIGHT;
    lcd->write_byte(lcd->addr, lcd->backlight);
}

void lcd_no_backlight(lcd_i2c_t *lcd)
{
    lcd->backlight = LCD_NOBACKLIGHT;
    lcd->write_byte(lcd->addr, lcd->backlight);
}

void lcd_display(lcd_i2c_t *lcd)
{
    lcd->displaycontrol |= LCD_DISPLAYON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_no_display(lcd_i2c_t *lcd)
{
    lcd->displaycontrol &= ~LCD_DISPLAYON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_cursor(lcd_i2c_t *lcd)
{
    lcd->displaycontrol |= LCD_CURSORON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_no_cursor(lcd_i2c_t *lcd)
{
    lcd->displaycontrol &= ~LCD_CURSORON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_blink(lcd_i2c_t *lcd)
{
    lcd->displaycontrol |= LCD_BLINKON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_no_blink(lcd_i2c_t *lcd)
{
    lcd->displaycontrol &= ~LCD_BLINKON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_scroll_display_left(lcd_i2c_t *lcd)
{
    lcd_command(lcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void lcd_scroll_display_right(lcd_i2c_t *lcd)
{
    lcd_command(lcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void lcd_left_to_right(lcd_i2c_t *lcd)
{
    lcd->displaymode |= LCD_ENTRYLEFT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}

void lcd_right_to_left(lcd_i2c_t *lcd)
{
    lcd->displaymode &= ~LCD_ENTRYLEFT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}

void lcd_autoscroll(lcd_i2c_t *lcd)
{
    lcd->displaymode |= LCD_ENTRYSHIFTINCREMENT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}

void lcd_no_autoscroll(lcd_i2c_t *lcd)
{
    lcd->displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}

void lcd_create_char(lcd_i2c_t *lcd, uint8_t location, const uint8_t charmap[8])
{
    location &= 0x7; // 0–7 only
    lcd_command(lcd, LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++)
        lcd_write(lcd, charmap[i]);
}
