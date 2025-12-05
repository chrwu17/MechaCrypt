/**
 * @file main.c
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Main file for MechaCrypt receiver MCU firmware.
 */

#include "../lib/main.h"


// ----------------- Local LCD handle -----------------
static lcd_i2c_t g_lcd;
volatile uint8_t buffer[48];
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


uint32_t get_millis(void) {
    return g_millis;
}
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

uint8_t fpga_read_message(uint8_t *buffer) {
    if (!digitalRead(PA8)) {
        return 0;
    }
    
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Starting SPI...");
    delay_millis(TIM15, 500);
    
    // Assert chip select
    digitalWrite(SPI_CE, 0);
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "CS asserted");
    delay_millis(TIM15, 500);
    
    // Read first byte only for testing
    uint8_t test_byte = spiSendReceive(0x00);
    
    lcd_set_cursor(&g_lcd, 0, 2);
    char hex[10];
    sprintf(hex, "Byte: %02X", test_byte);
    lcd_print(&g_lcd, hex);
    
    // Read remaining bytes
    buffer[0] = test_byte;
    for (int i = 1; i < 16; i++) {
        buffer[i] = spiSendReceive(0x00);
    }
    
    delay_millis(TIM15, 100);
    digitalWrite(SPI_CE, 1);
    
    lcd_set_cursor(&g_lcd, 0, 3);
    lcd_print(&g_lcd, "CS deasserted");
    delay_millis(TIM15, 2000);
    
    return 1;
}

volatile   uint8_t received_data[16];
void inject_test_ciphertext(void) {
    // Wait for ready signal from FPGA
    uint32_t timeout = get_millis() + 10000;
    while (!digitalRead(PA8)) {
        if (get_millis() > timeout) {
            lcd_clear(&g_lcd);
            lcd_print(&g_lcd, "Timeout waiting");
            return;
        }
    }
    
    lcd_clear(&g_lcd);
    lcd_print(&g_lcd, "Reading FPGA...");
    
    
    // Assert CS
    digitalWrite(SPI_CE, 0);
    delay_millis(TIM15, 5);
    
    // Read 16 bytes
    for (int i = 0; i < 16; i++) {
        received_data[i] = spiSendReceive(0x00);
        delay_millis(TIM15, 1);
    }
    
    delay_millis(TIM15, 5);
    digitalWrite(SPI_CE, 1);
    
    // *** KEY CHANGE: Store to shared state ***
    receiver_store_block(0, received_data);  // Store as block 0
    
    // Display on LCD
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Block stored!");
    
    lcd_set_cursor(&g_lcd, 0, 1);
    char hex_str[4];
    for (int i = 0; i < 8; i++) {
        sprintf(hex_str, "%02X ", received_data[i]);
        lcd_print(&g_lcd, hex_str);
    }
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
void demo_progress_bar_nonblocking(void) {
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
                reset_progress(&g_lcd);
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

    // receiver_demo_init_plaintext();  // Sample demo plaintext data for Midpoint check in

    

    // Initialize SPI for FPGA communication
    initSPI(0b111, 0, 0); 
    
     

    // Chip select pin for FPGA
    pinMode(SPI_CE, GPIO_OUTPUT);
    digitalWrite(SPI_CE, 1);  // idle high (not selected)
    pinMode(PA8, GPIO_INPUT);
    GPIOA->PUPDR &= ~(0b11 << (8 * 2));  // Clear bits
    GPIOA->PUPDR |= (0b10 << (8 * 2));   // Set pull-down (10)

    inject_test_ciphertext();
    

    // Add to main.c after inject_test_ciphertext() call:
    delay_millis(TIM15, 300);   // give ESP time to stabilize

    
    // Main loop:
    //  - service HTTP
    //  - SPI fetches blocks from FPGA
    while (1) {
        processWebRequest(USART);

        receiver_spi_demo_poll(); // Uncomment to enable SPI fetching
        //demo_progress_bar_nonblocking();
        

        if (get_received_block_count() >= get_total_expected_blocks() && get_total_expected_blocks() > 0) {
            // All blocks received
            lcd_set_cursor(&g_lcd, 0, 3);
            lcd_print(&g_lcd, "Transfer Complete!   ");
        }
    }
}