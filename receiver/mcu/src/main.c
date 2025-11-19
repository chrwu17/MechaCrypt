/**
 * @file main.c
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Main file for MechaCrypt receiver MCU firmware.
 */

#include "../lib/main.h"


// ----------------- Local LCD handle -----------------
static lcd_i2c_t lcd;

// ----------------- Simple microsecond delay using SysTick -----------------
void delay_us(uint32_t us)
{
    if (us == 0) return;

    uint32_t ticks = (SystemCoreClock / 1000000u) * us;
    if (ticks > 0xFFFFFFu) ticks = 0xFFFFFFu;

    SysTick->LOAD = ticks;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {
        // spin
    }
    SysTick->CTRL = 0;
}



static void lcd_hw_init(void)
{
    // Initialize hardware I2C1
    initI2C1();
    
    // Initialize LCD using hardware I2C write function
    lcd_init(&lcd, 0x27, 20, 4, i2c_write_byte, delay_us);
    lcd_begin(&lcd);
    lcd_backlight(&lcd);
    lcd_clear(&lcd);
    lcd_set_cursor(&lcd, 0, 0);
    lcd_print(&lcd, "MechaCrypt!");
}

// ----------------- Main entry -----------------
int main(void)
{
    // Core clocks
    configureFlash();
    configureClock();

    // GPIO clocks
    gpioEnable(GPIO_PORT_A);
    gpioEnable(GPIO_PORT_B);
    gpioEnable(GPIO_PORT_C);

    // Status LED (no blinking, just configured in case you want it later)
    pinMode(LED_PIN, GPIO_OUTPUT);

    // Timer for delay_millis (used elsewhere in project)
    initTIM(TIM15);

    // Initiallize USART1 for ESP8266
    USART_TypeDef *USART = initUSART(USART1_ID, 125000);

    // --- Configure USART1 to PB6 (TX) / PB7 (RX) ---

    // Configure PB6/PB7 as alternate function pins
    pinMode(PB6, GPIO_ALT);
    pinMode(PB7, GPIO_ALT);

    // Clear existing AF bits for pins 6 and 7
    GPIOB->AFR[0] &= ~((0xF << GPIO_AFRL_AFSEL6_Pos) |
                       (0xF << GPIO_AFRL_AFSEL7_Pos));

    // Set AF7 (USART1) for PB6 and PB7
    GPIOB->AFR[0] |= (0b0111 << GPIO_AFRL_AFSEL6_Pos) |
                     (0b0111 << GPIO_AFRL_AFSEL7_Pos);

    // Call LCD initialization function
    lcd_hw_init();

    receiver_demo_init_plaintext();  // Sample demo plaintext data for Midpoint check in

    // Initialize SPI for FPGA communication
    initSPI(0b111, 0, 0);  

    // Chip select pin for FPGA
    pinMode(SPI_CE, GPIO_OUTPUT);
    digitalWrite(SPI_CE, 1);  // idle high (not selected)


    // Main loop:
    //  - service HTTP
    //  - SPI fetches blocks from FPGA
    while (1) {
        processWebRequest(USART);

        // receiver_spi_demo_poll(); // Uncomment to enable SPI fetching
    }
}
