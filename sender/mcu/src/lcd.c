// STM32L432KC_I2C_LCD.c
// Source code for I2C LCD 20x4 functions

#include "../lib/lcd.h"
#include "../lib/STM32L432KC_RCC.h"
#include "../lib/STM32L432KC_GPIO.h"

static uint8_t backlight_state = LCD_BACKLIGHT;

// Delay functions
static void delay_us(uint32_t us) {
    // At 80 MHz, approximately 80 cycles per microsecond
    for(uint32_t i = 0; i < us * 20; i++) {
        __NOP();
    }
}

static void delay_ms(uint32_t ms) {
    for(uint32_t i = 0; i < ms; i++) {
        delay_us(1000);
    }
}

// Initialize I2C1: PB6 (SCL), PB7 (SDA)
void initI2C1(void) {
    // Enable GPIOB and I2C1 clocks
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    
    // Configure PB6 and PB7 as alternate function (AF4 for I2C1)
    pinMode(PB6, GPIO_ALT);
    pinMode(PB7, GPIO_ALT);
    
    // Set alternate function to AF4
    GPIOB->AFR[0] |= (4 << GPIO_AFRL_AFSEL6_Pos);
    GPIOB->AFR[0] |= (4 << GPIO_AFRL_AFSEL7_Pos);
    
    // Set open-drain output type
    GPIOB->OTYPER |= (GPIO_OTYPER_OT6 | GPIO_OTYPER_OT7);
    
    // Set high speed
    GPIOB->OSPEEDR |= (GPIO_OSPEEDR_OSPEED6 | GPIO_OSPEEDR_OSPEED7);
    
    // Enable pull-up resistors
    GPIOB->PUPDR &= ~(0b11 << GPIO_PUPDR_PUPD6_Pos);
    GPIOB->PUPDR |= (0b01 << GPIO_PUPDR_PUPD6_Pos);
    GPIOB->PUPDR &= ~(0b11 << GPIO_PUPDR_PUPD7_Pos);
    GPIOB->PUPDR |= (0b01 << GPIO_PUPDR_PUPD7_Pos);
    
    // Disable I2C1
    I2C1->CR1 &= ~I2C_CR1_PE;
    
    // Configure I2C timing for 100 kHz (assuming 80 MHz PCLK1)
    I2C1->TIMINGR = 0x10909CEC; // 100 kHz @ 80 MHz
    
    // Enable I2C1
    I2C1->CR1 |= I2C_CR1_PE;
}

// Low-level I2C write function
static void i2c_write_byte(uint8_t data) {
    // Wait until I2C is ready
    while(I2C1->ISR & I2C_ISR_BUSY);
    
    // Configure transfer: 1 byte to LCD address
    I2C1->CR2 = 0;
    I2C1->CR2 |= ((uint32_t)LCD_I2C_ADDR << 1);
    I2C1->CR2 |= (1 << I2C_CR2_NBYTES_Pos);
    I2C1->CR2 |= I2C_CR2_AUTOEND;
    I2C1->CR2 |= I2C_CR2_START;
    
    // Send data
    while(!(I2C1->ISR & I2C_ISR_TXIS));
    I2C1->TXDR = data;
    
    // Wait for transfer complete
    while(!(I2C1->ISR & I2C_ISR_STOPF));
    I2C1->ICR |= I2C_ICR_STOPCF;
}

// Expander write with backlight
static void expanderWrite(uint8_t data) {
    i2c_write_byte(data | backlight_state);
}

// Pulse enable pin
static void pulseEnable(uint8_t data) {
    expanderWrite(data | LCD_EN);  // EN high
    delay_us(1);                    // Enable pulse must be >450ns
    
    expanderWrite(data & ~LCD_EN); // EN low
    delay_us(50);                   // Commands need >37us to settle
}

// Write 4 bits to LCD
static void write4bits(uint8_t value) {
    expanderWrite(value);
    pulseEnable(value);
}

// Send command or data (internal function)
static void send(uint8_t value, uint8_t mode) {
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;
    write4bits(highnib | mode);
    write4bits(lownib | mode);
}

// Send command to LCD
void lcd_send_cmd(uint8_t cmd) {
    send(cmd, 0);
    if(cmd == LCD_CMD_CLEAR || cmd == LCD_CMD_HOME) {
        delay_us(2000); // Clear and home commands need more time
    }
}

// Send data to LCD
void lcd_send_data(uint8_t data) {
    send(data, LCD_RS);
}

// Initialize LCD
void lcd_init(void) {
    delay_ms(50); // Wait for LCD to power up (needs >40ms after Vcc rises to 2.7V)
    
    // Reset expander and turn backlight off initially
    expanderWrite(backlight_state);
    delay_ms(1000);
    
    // Put the LCD into 4 bit mode
    // This is according to the Hitachi HD44780 datasheet figure 24, page 46
    
    // Start in 8bit mode, try to set 4 bit mode
    write4bits(0x03 << 4);
    delay_us(4500); // Wait min 4.1ms
    
    // Second try
    write4bits(0x03 << 4);
    delay_us(4500); // Wait min 4.1ms
    
    // Third go!
    write4bits(0x03 << 4);
    delay_us(150);
    
    // Finally, set to 4-bit interface
    write4bits(0x02 << 4);
    
    // Now we can use the normal command function
    // Set # lines, font size, etc.
    lcd_send_cmd(LCD_CMD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2LINE | LCD_5x8_DOTS);
    
    // Turn the display on with no cursor or blinking default
    lcd_send_cmd(LCD_CMD_DISPLAY_CTRL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF);
    
    // Clear display
    lcd_clear();
    
    // Initialize to default text direction (for left-to-right languages)
    lcd_send_cmd(LCD_CMD_ENTRY_MODE | LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DEC);
    
    // Return home
    lcd_send_cmd(LCD_CMD_HOME);
    
    // Turn on backlight
    backlight_state = LCD_BACKLIGHT;
    expanderWrite(0);
}

// Clear LCD display
void lcd_clear(void) {
    lcd_send_cmd(LCD_CMD_CLEAR);
    delay_us(2000);
}

// Return cursor to home position
void lcd_home(void) {
    lcd_send_cmd(LCD_CMD_HOME);
    delay_us(2000);
}

// Set cursor position (row: 0-3, col: 0-19)
void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if(row >= LCD_ROWS) {
        row = LCD_ROWS - 1;
    }
    lcd_send_cmd(LCD_CMD_SET_DDRAM_ADDR | (col + row_offsets[row]));
}

// Print string at current cursor position
void lcd_print(char *str) {
    while(*str) {
        lcd_send_data(*str++);
    }
}

// Print string at specific position
void lcd_print_at(uint8_t row, uint8_t col, char *str) {
    lcd_set_cursor(row, col);
    lcd_print(str);
}

// Turn backlight on
void lcd_backlight_on(void) {
    backlight_state = LCD_BACKLIGHT;
    expanderWrite(0);
}

// Turn backlight off
void lcd_backlight_off(void) {
    backlight_state = LCD_NO_BACKLIGHT;
    expanderWrite(0);
}

// Create custom characters for progress bar
void lcd_create_progress_chars(void) {
    // Custom character patterns for progress bar (0-5 bars)
    uint8_t chars[6][8] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Empty
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, // 1 bar
        {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}, // 2 bars
        {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C}, // 3 bars
        {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E}, // 4 bars
        {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}  // 5 bars (full)
    };
    
    for(uint8_t i = 0; i < 6; i++) {
        lcd_send_cmd(LCD_CMD_SET_CGRAM_ADDR | (i << 3));
        for(uint8_t j = 0; j < 8; j++) {
            lcd_send_data(chars[i][j]);
        }
    }
    
    lcd_send_cmd(LCD_CMD_SET_DDRAM_ADDR); // Return to DDRAM
}

// Display progress bar on specified row (0-100%)
void lcd_show_progress(uint8_t row, uint8_t percent) {
    if(percent > 100) percent = 100;
    
    // Progress bar is 20 characters wide
    uint16_t total_bars = (percent * 100) / 100; // Total bars out of 100 (20 chars * 5 bars each)
    uint8_t full_chars = total_bars / 5;
    uint8_t remainder = total_bars % 5;
    
    lcd_set_cursor(row, 0);
    
    // Draw full characters
    for(uint8_t i = 0; i < full_chars && i < 20; i++) {
        lcd_send_data(5); // Full block
    }
    
    // Draw partial character
    if(full_chars < 20) {
        lcd_send_data(remainder);
    }
    
    // Fill remaining with empty characters
    for(uint8_t i = full_chars + 1; i < 20; i++) {
        lcd_send_data(0); // Empty
    }
}