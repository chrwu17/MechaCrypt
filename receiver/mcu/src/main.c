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

// Global millisecond counter (overflows after ~49 days)
volatile uint32_t g_millis = 0;

/**
 * @brief Initialize TIM15 for 1ms interrupts
 */
void initTIM15_millis(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    
    // Assuming 80 MHz system clock
    // Prescaler = 7999 gives 10 kHz (0.1ms per tick)
    // ARR = 9 gives interrupt every 1ms
    TIM15->PSC = 7999;   // (80,000,000 / (7999+1)) = 10,000 Hz
    TIM15->ARR = 9;      // (10,000 / (9+1)) = 1,000 Hz (1ms)
    
    // Enable update interrupt
    TIM15->DIER |= TIM_DIER_UIE;
    
    // Enable TIM15 interrupt in NVIC
    NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);
    NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, 3);
    
    // Start timer
    TIM15->CR1 |= TIM_CR1_CEN;
}

/**
 * @brief TIM15 interrupt handler - increments millisecond counter
 */
void TIM1_BRK_TIM15_IRQHandler(void) {
    if (TIM15->SR & TIM_SR_UIF) {
        TIM15->SR &= ~TIM_SR_UIF;  // Clear flag
        g_millis++;
    }
}

/**
 * @brief Get current millisecond count
 */
uint32_t get_millis(void) {
    return g_millis;
}void demo_progress_bar_nonblocking(void) {
    static uint32_t last_update = 0;
    static int block_count = 0;
    static uint8_t demo_started = 0;
    
    // Start demo if not started
    if (!demo_started) {
        set_expected_transfer_size(256);
        lcd_update_transfer_status(&g_lcd, 0, get_total_expected_blocks());
        demo_started = 1;
        last_update = get_millis();
        return;
    }
    
    // Check if 800ms has elapsed
    uint32_t now = get_millis();
    if (now - last_update >= 800) {
        if (block_count < 16) {
            on_block_received(&g_lcd);
            block_count++;
            last_update = now;
        } else {
            // Demo complete - reset after 3 seconds
            if (now - last_update >= 3800) {
                reset_progress();
                block_count = 0;
                demo_started = 0;
            }
        }
    }
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

    initTIM15_millis();

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
        demo_progress_bar_nonblocking();

        if (get_received_block_count() >= get_total_expected_blocks() && get_total_expected_blocks() > 0) {
            // All blocks received
            lcd_set_cursor(&g_lcd, 0, 3);
            lcd_print(&g_lcd, "Transfer Complete!   ");
        }
    }
}
