/**
 * @file main.c
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Main file for MechaCrypt receiver MCU firmware.
 */

#include "../lib/main.h"


// ----------------- Local LCD handle -----------------
static lcd_i2c_t g_lcd;

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

/**
 * @brief Demo function to test the progress bar
 * Call this from main() to see the progress bar in action
 */
void demo_progress_bar(void) {
    // Simulate receiving 16 blocks (256 bytes total)
    set_expected_transfer_size(256);
    lcd_update_transfer_status(&g_lcd, 0, get_total_expected_blocks());
    
    delay_millis(TIM15, 1000);  // Pause before starting
    
    // Simulate receiving blocks one by one
    for (int i = 0; i < 16; i++) {
        delay_millis(TIM15, 800);  // Wait 800ms between blocks
        on_block_received(&g_lcd);  // Update progress
    }
    
    // Show completion for 3 seconds
    delay_millis(TIM15, 3000);
    
    // Reset for next demo cycle
    reset_progress();
}

static void lcd_hw_init(void)
{
    // Initialize hardware I2C1
    initI2C1();
    
    // Initialize LCD using hardware I2C write function
    lcd_init(&g_lcd, 0x27, 20, 4, i2c_write_byte, delay_us);
    lcd_begin(&g_lcd);
    lcd_backlight(&g_lcd);
    lcd_init_progress_chars(&g_lcd);
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_update_transfer_status(&g_lcd, 0, 0);
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

    initMsgReceive();

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
        demo_progress_bar();

        if (get_received_block_count() >= get_total_expected_blocks() && get_total_expected_blocks() > 0) {
            // All blocks received
            lcd_set_cursor(&g_lcd, 0, 3);
            lcd_print(&g_lcd, "Transfer Complete!   ");
        }
    }
}
