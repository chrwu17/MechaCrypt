#include "../lib/lcd_i2c.h"

/* ====== small local helpers ====== */

static void _delay_us(const lcd_i2c_t *lcd, uint32_t us) {
    if (lcd->delay_us) { lcd->delay_us(us); return; }
    /* Fallback: simple CPU-busy loop.
       Tune the multiplier for your clock if timing is too short/long.
       On ~80 MHz core, ~4-5 NOPs per µs is a ballpark; we’ll overshoot a bit. */
    volatile uint32_t cnt = us * 50u;
    while (cnt--) { __asm volatile("nop"); }
}

static int _write8(const lcd_i2c_t *lcd, uint8_t b) {
    return lcd->write_byte(lcd->addr, (uint8_t)(b | lcd->backlight));
}

/* PCF8574 EN pulse */
static void _pulse_enable(const lcd_i2c_t *lcd, uint8_t data) {
    _write8(lcd, data | LCD_EN);
    _delay_us(lcd, 1);        /* >450 ns */
    _write8(lcd, data & ~LCD_EN);
    _delay_us(lcd, 50);       /* >37 us for commands to settle */
}

/* Send upper/lower nibble (D7..D4) over PCF8574 */
static void _write4bits(const lcd_i2c_t *lcd, uint8_t value) {
    _write8(lcd, value);
    _pulse_enable(lcd, value);
}

/* Send either command (mode=0) or data (mode=LCD_RS) */
static void _send(const lcd_i2c_t *lcd, uint8_t value, uint8_t mode_rs) {
    uint8_t hi = (value & 0xF0) | mode_rs;
    uint8_t lo = ((value << 4) & 0xF0) | mode_rs;
    _write4bits(lcd, hi);
    _write4bits(lcd, lo);
}

/* ====== public API ====== */

void lcd_init(lcd_i2c_t *lcd, uint8_t addr7, uint8_t cols, uint8_t rows,
              lcd_i2c_write_fn write_cb, lcd_delay_us_fn delay_cb)
{
    lcd->addr            = addr7;
    lcd->cols            = cols;
    lcd->rows            = rows;
    lcd->numlines        = rows;
    lcd->displayfunction = (rows > 1) ? (LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS)
                                      : (LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS);
    lcd->displaycontrol  = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    lcd->displaymode     = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    lcd->backlight       = LCD_NOBACKLIGHT; /* match Arduino default at power-up */
    lcd->write_byte      = write_cb;
    lcd->delay_us        = delay_cb;
}

void lcd_begin(lcd_i2c_t *lcd)
{
    /* According to HD44780 spec, wait 40+ ms after Vcc rises above 2.7V */
    _delay_us(lcd, 50000);

    /* Reset expander (also sets current backlight state) */
    _write8(lcd, 0x00);
    _delay_us(lcd, 1000);

    /* Initialization sequence to enter 4-bit mode (same as Arduino lib) */
    _write4bits(lcd, 0x30);  _delay_us(lcd, 4500);
    _write4bits(lcd, 0x30);  _delay_us(lcd, 4500);
    _write4bits(lcd, 0x30);  _delay_us(lcd, 150);
    _write4bits(lcd, 0x20);  /* finally 4-bit */

    /* Function set */
    lcd_command(lcd, LCD_FUNCTIONSET | lcd->displayfunction);

    /* Display on/off control */
    lcd_display(lcd);

    /* Clear display */
    lcd_clear(lcd);

    /* Entry mode set */
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);

    /* Home position */
    lcd_home(lcd);
}

void lcd_clear(lcd_i2c_t *lcd) {
    lcd_command(lcd, LCD_CLEARDISPLAY);
    _delay_us(lcd, 2000);
}

void lcd_home(lcd_i2c_t *lcd) {
    lcd_command(lcd, LCD_RETURNHOME);
    _delay_us(lcd, 2000);
}

void lcd_set_cursor(lcd_i2c_t *lcd, uint8_t col, uint8_t row) {
    static const uint8_t row_offsets[4] = { 0x00, 0x40, 0x14, 0x54 };
    if (row >= lcd->numlines) row = lcd->numlines - 1;
    lcd_command(lcd, (uint8_t)(LCD_SETDDRAMADDR | (col + row_offsets[row])));
}

void lcd_display(lcd_i2c_t *lcd) {
    lcd->displaycontrol |= LCD_DISPLAYON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}
void lcd_no_display(lcd_i2c_t *lcd) {
    lcd->displaycontrol &= ~LCD_DISPLAYON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_cursor(lcd_i2c_t *lcd) {
    lcd->displaycontrol |= LCD_CURSORON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}
void lcd_no_cursor(lcd_i2c_t *lcd) {
    lcd->displaycontrol &= ~LCD_CURSORON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_blink(lcd_i2c_t *lcd) {
    lcd->displaycontrol |= LCD_BLINKON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}
void lcd_no_blink(lcd_i2c_t *lcd) {
    lcd->displaycontrol &= ~LCD_BLINKON;
    lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->displaycontrol);
}

void lcd_scroll_display_left(lcd_i2c_t *lcd) {
    lcd_command(lcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void lcd_scroll_display_right(lcd_i2c_t *lcd) {
    lcd_command(lcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void lcd_left_to_right(lcd_i2c_t *lcd) {
    lcd->displaymode |= LCD_ENTRYLEFT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}
void lcd_right_to_left(lcd_i2c_t *lcd) {
    lcd->displaymode &= ~LCD_ENTRYLEFT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}

void lcd_autoscroll(lcd_i2c_t *lcd) {
    lcd->displaymode |= LCD_ENTRYSHIFTINCREMENT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}
void lcd_no_autoscroll(lcd_i2c_t *lcd) {
    lcd->displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
    lcd_command(lcd, LCD_ENTRYMODESET | lcd->displaymode);
}

void lcd_create_char(lcd_i2c_t *lcd, uint8_t location, const uint8_t charmap[8]) {
    location &= 0x7;
    lcd_command(lcd, (uint8_t)(LCD_SETCGRAMADDR | (location << 3)));
    for (int i = 0; i < 8; i++) {
        lcd_write(lcd, charmap[i]);
    }
}

/* Backlight control is just cached bit that gets OR’d into every PCF8574 write */
void lcd_backlight(lcd_i2c_t *lcd) {
    lcd->backlight = LCD_BACKLIGHT;
    _write8(lcd, 0x00); /* poke expander to take effect immediately */
}
void lcd_no_backlight(lcd_i2c_t *lcd) {
    lcd->backlight = LCD_NOBACKLIGHT;
    _write8(lcd, 0x00);
}

/* Command/Data primitives */
void lcd_command(lcd_i2c_t *lcd, uint8_t value) {
    _send(lcd, value, 0); /* RS=0 */
}

void lcd_write(lcd_i2c_t *lcd, uint8_t value) {
    _send(lcd, value, LCD_RS); /* RS=1 */
}

void lcd_print(lcd_i2c_t *lcd, const char *s) {
    if (!s) return;
    while (*s) {
        lcd_write(lcd, (uint8_t)*s++);
    }
}

void lcd_print_n(lcd_i2c_t *lcd, const char *s, size_t n) {
    if (!s) return;
    for (size_t i = 0; i < n; ++i) {
        lcd_write(lcd, (uint8_t)s[i]);
    }
}
