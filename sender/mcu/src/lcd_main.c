// main.c — STM32L432KC + I2C1 (register-level, no LL/HAL) + PCF8574 LCD (20x4)
// Pins: PB8=SCL, PB9=SDA, AF4, open-drain, pull-up. Backpack addr usually 0x27 or 0x3F.

#include "stm32l4xx.h"
#include "../lib/lcd_i2c.h"  // from the C driver provided earlier (no HAL/LL inside)

// --- µs delay using SysTick (no LL/HAL, no DWT) ---
static void delay_us(uint32_t us) {
    if (us == 0) return;
    // SysTick is a 24-bit downcounter; one-shot for the requested microseconds
    uint32_t ticks_per_us = SystemCoreClock / 1000000U;
    uint32_t load = ticks_per_us * us;
    if (load == 0) load = 1;                // ensure at least 1 tick
    if (load > 0xFFFFFFU) {                 // cap to 24-bit max
        // For very long delays, loop in chunks
        uint32_t full = load / 0xFFFFFFU;
        uint32_t rem  = load % 0xFFFFFFU;
        for (uint32_t i = 0; i < full; ++i) {
            SysTick->LOAD = 0xFFFFFFU;
            SysTick->VAL  = 0;
            SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
            while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {}
            SysTick->CTRL = 0;
        }
        if (rem) {
            SysTick->LOAD = rem;
            SysTick->VAL  = 0;
            SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
            while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {}
            SysTick->CTRL = 0;
        }
        return;
    }
    SysTick->LOAD = load - 1;               // COUNTFLAG sets when it hits zero
    SysTick->VAL  = 0;                      // clear current value + COUNTFLAG
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {}
    SysTick->CTRL = 0;                      // stop SysTick
}


// --------- I2C1 init @100 kHz using HSI16 as kernel clock (no LL) ----------
static void I2C1_Init_100k_HSI16(void)
{
    // 1) Enable clocks
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;        // GPIOB clock
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;        // I2C1 clock

    // 2) Turn on HSI16 and select it as I2C1 kernel clock
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {}
    // I2C1SEL bits in CCIPR: 00 = HSI16
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_I2C1SEL_Msk) | (0u << RCC_CCIPR_I2C1SEL_Pos);

    // 3) PB8/PB9 as AF4 open-drain, pull-up, very high speed
    // MODER: 10 = Alternate
    GPIOB->MODER   &= ~((3u<<(8*2)) | (3u<<(9*2)));
    GPIOB->MODER   |=  ((2u<<(8*2)) | (2u<<(9*2)));
    // OTYPER: 1 = open-drain
    GPIOB->OTYPER  |=  (1u<<8) | (1u<<9);
    // OSPEEDR: 11 = very high
    GPIOB->OSPEEDR |=  (3u<<(8*2)) | (3u<<(9*2));
    // PUPDR: 01 = pull-up
    GPIOB->PUPDR   &= ~((3u<<(8*2)) | (3u<<(9*2)));
    GPIOB->PUPDR   |=  (1u<<(8*2)) | (1u<<(9*2));
    // AFRH for pins 8..15: AF4 (I2C1)
    GPIOB->AFR[1]  &= ~((0xFu<<((8-8)*4)) | (0xFu<<((9-8)*4)));
    GPIOB->AFR[1]  |=  ((4u<<((8-8)*4)) | (4u<<((9-8)*4)));

    // 4) Reset/disable I2C1 before configuring
    I2C1->CR1 &= ~I2C_CR1_PE;
    RCC->APB1RSTR1 |=  RCC_APB1RSTR1_I2C1RST;
    RCC->APB1RSTR1 &= ~RCC_APB1RSTR1_I2C1RST;

    // 5) TIMINGR for ~100 kHz with 16 MHz I2C clock (HSI16)
    //    This value is a solid, conservative default on STM32L4 at 100 kHz.
    I2C1->TIMINGR = 0x00303D5B;

    // 6) Filters: analog ON (ANFOFF=0), digital OFF (DNF=0)
    I2C1->CR1 &= ~I2C_CR1_ANFOFF;                 // 0 = analog filter enabled
    I2C1->CR1 &= ~I2C_CR1_DNF;                    // DNF = 0

    // 7) Enable I2C1
    I2C1->CR1 |= I2C_CR1_PE;
}

// --------- One-byte PCF8574 write via I2C1 (blocking, no LL) ----------
static int pcf8574_write_regs(uint8_t addr7, uint8_t byte)
{
    // Wait if bus is busy
    while (I2C1->ISR & I2C_ISR_BUSY) {}

    // Program CR2:
    // - SADD (7-bit): left-aligned in bits [7:1]
    // - NBYTES = 1
    // - RD_WRN = 0 (write)
    // - AUTOEND = 1 (generate STOP after NBYTES)
    // - START = 1
    I2C1->CR2 = ( (uint32_t)(addr7 << 1) & I2C_CR2_SADD ) |
                ( (1u << I2C_CR2_NBYTES_Pos) & I2C_CR2_NBYTES ) |
                (0u << I2C_CR2_RD_WRN_Pos) |
                I2C_CR2_AUTOEND |
                I2C_CR2_START;

    // Wait for TXIS (ready to transmit) or NACKF
    while ((I2C1->ISR & I2C_ISR_TXIS) == 0) {
        if (I2C1->ISR & I2C_ISR_NACKF) {
            I2C1->ICR = I2C_ICR_NACKCF;          // clear NACK
            // Also wait/clear STOP if it comes
            while ((I2C1->ISR & I2C_ISR_STOPF) == 0) {}
            I2C1->ICR = I2C_ICR_STOPCF;
            return -1; // address NACK
        }
    }

    // Write the byte
    I2C1->TXDR = byte;

    // Wait for STOP (transfer complete) or NACK
    while ((I2C1->ISR & I2C_ISR_STOPF) == 0) {
        if (I2C1->ISR & I2C_ISR_NACKF) {
            I2C1->ICR = I2C_ICR_NACKCF;
            while ((I2C1->ISR & I2C_ISR_STOPF) == 0) {}
            I2C1->ICR = I2C_ICR_STOPCF;
            return -2; // data NACK
        }
    }
    // Clear STOP flag
    I2C1->ICR = I2C_ICR_STOPCF;
    return 0;
}

// --------- App ----------
int main(void)
{
    SystemCoreClockUpdate(); // keep SystemCoreClock correct for DWT delay

    I2C1_Init_100k_HSI16();  // PB8/PB9, HSI16 kernel clock, 100 kHz

    // LCD bring-up
    lcd_i2c_t lcd;
    lcd_init(&lcd,
             0x27,                 // change to 0x3F if your backpack uses that
             20,                   // columns
             4,                    // rows
             pcf8574_write_regs,   // low-level 1-byte write
             delay_us);            // µs delay
    lcd_begin(&lcd);
    lcd_backlight(&lcd);

    // HelloWorld (from your .pde)
    lcd_set_cursor(&lcd, 3, 0);  lcd_print(&lcd, "Hello, world!");
    lcd_set_cursor(&lcd, 2, 1);  lcd_print(&lcd, "Ywrobot Arduino!");
    lcd_set_cursor(&lcd, 0, 2);  lcd_print(&lcd, "Arduino LCM IIC 2004");
    lcd_set_cursor(&lcd, 2, 3);  lcd_print(&lcd, "Power By Ec-yuan!");

    while (1) { __WFI(); }
}
