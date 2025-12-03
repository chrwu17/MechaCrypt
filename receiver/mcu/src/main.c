/**
 * @file main.c
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Main file with integrated mechanical block reception
 */

#include "../lib/main.h"
#include <string.h>  // For memset

// ----------------- Local LCD handle -----------------
static lcd_i2c_t g_lcd;

// ----------------- Debug/Status Variables -----------------
volatile uint32_t blocks_processed = 0;
volatile uint32_t last_interrupt_count = 0;

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

// Global millisecond counter
volatile uint32_t g_millis = 0;

/**
 * @brief Initialize TIM15 for 1ms interrupts
 */
void initTIM15_millis(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    
    // Assuming 80 MHz system clock
    TIM15->PSC = 7999;   // 10 kHz
    TIM15->ARR = 9;      // 1 kHz (1ms)
    
    TIM15->DIER |= TIM_DIER_UIE;
    
    NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);
    NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, 3);
    
    TIM15->CR1 |= TIM_CR1_CEN;
}

/**
 * @brief TIM15 interrupt handler
 */
void TIM1_BRK_TIM15_IRQHandler(void) {
    if (TIM15->SR & TIM_SR_UIF) {
        TIM15->SR &= ~TIM_SR_UIF;
        g_millis++;
    }
}

/**
 * @brief Get current millisecond count
 */
uint32_t get_millis(void) {
    return g_millis;
}

/**
 * @brief Initialize LCD hardware
 */
static void lcd_hw_init(void)
{
    initI2C1();
    lcd_init(&g_lcd, 0x27, 20, 4, i2c_write_byte, delay_us);
    lcd_begin(&g_lcd);
    lcd_backlight(&g_lcd);
    lcd_init_progress_chars(&g_lcd);
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_update_transfer_status(&g_lcd, 0, 0);
}

/**
 * @brief Process received message blocks from mechanical system
 * Call this in main loop to check for new blocks
 */
void process_received_blocks(void) {
    if (messageReceivedFlag) {
        messageReceivedFlag = 0;
        digitalWrite(LED_PIN, 1);
        
        // ✅ CORRECT: Decrypt first, then store plaintext
        uint8_t plaintext[16];
        int result = fpgaDecryptBlock(
            (const uint8_t*)receivedMessage,  // ciphertext from mechanical system
            plaintext                          // output plaintext
        );
        
        if (result == 0) {
            // Decryption successful
            receiver_store_block(blocks_processed, plaintext);
            on_block_received(&g_lcd);
            blocks_processed++;
        } else {
            // Decryption failed - log error or retry
            // For now, maybe just skip this block
        }
        
        digitalWrite(LED_PIN, 0);
    }
}

/**
 * @brief Display debug info on LCD (for testing)
 */
void display_debug_info(void) {
    static uint32_t last_debug_update = 0;
    uint32_t now = get_millis();
    
    // Update every 500ms
    if (now - last_debug_update < 500) {
        return;
    }
    last_debug_update = now;
    
    char buf[21];
    
    // Line 0: Status
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "MechaCrypt RX Debug ");
    
    // Line 1: Interrupt count
    lcd_set_cursor(&g_lcd, 0, 1);
    uint32_t isr_count = getInterruptCount();
    snprintf(buf, sizeof(buf), "ISR: %5lu Blk:%3lu", 
             (unsigned long)isr_count, 
             (unsigned long)blocks_processed);
    lcd_print(&g_lcd, buf);
    
    // Line 2: Current byte position
    lcd_set_cursor(&g_lcd, 0, 2);
    uint8_t byte_idx = getCurrentByteIndex();
    uint8_t last_byte = getLastByteValue();
    snprintf(buf, sizeof(buf), "Pos:%2u Last:0x%02X   ", byte_idx, last_byte);
    lcd_print(&g_lcd, buf);
    
    // Line 3: Change detection
    lcd_set_cursor(&g_lcd, 0, 3);
    if (isr_count != last_interrupt_count) {
        lcd_print(&g_lcd, "RECEIVING...        ");
        last_interrupt_count = isr_count;
    } else {
        lcd_print(&g_lcd, "Idle                ");
    }
}

/**
 * @brief Test mode - display first received byte values
 */
void display_test_mode(void) {
    static uint32_t last_update = 0;
    uint32_t now = get_millis();
    
    if (now - last_update < 1000) return;
    last_update = now;
    
    char buf[21];
    
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Test Mode - Waiting ");
    
    lcd_set_cursor(&g_lcd, 0, 1);
    snprintf(buf, sizeof(buf), "ISR:%lu B:%u       ", 
             (unsigned long)getInterruptCount(),
             getCurrentByteIndex());
    lcd_print(&g_lcd, buf);
    
    // Show first 8 bytes of current block
    lcd_set_cursor(&g_lcd, 0, 2);
    for (int i = 0; i < 8 && i < MSG_BYTES; i++) {
        snprintf(buf, 4, "%02X ", receivedMessage[i]);
        lcd_print(&g_lcd, buf);
    }
    
    lcd_set_cursor(&g_lcd, 0, 3);
    for (int i = 8; i < 16 && i < MSG_BYTES; i++) {
        snprintf(buf, 4, "%02X ", receivedMessage[i]);
        lcd_print(&g_lcd, buf);
    }
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

    // Status LED
    pinMode(LED_PIN, GPIO_OUTPUT);
    digitalWrite(LED_PIN, 0);

    // Timer for delays
    initTIM(TIM15);
    initTIM15_millis();

    // Initialize USART1 for ESP8266
    USART_TypeDef *USART = initUSART(USART1_ID, 125000);

    // Initialize LCD
    lcd_hw_init();

    // Show startup message
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "MechaCrypt Receiver ");
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Initializing...     ");
    delay_millis(TIM15, 1000);

    // Initialize mechanical message receiver (CRITICAL!)
    initMsgReceive();

    // Clear demo data and prepare for real reception
    for (int i = 0; i < MAX_BLOCKS; i++) {
        memset((void*)received_blocks[i], 0, 16);
        have_received[i] = 0;
    }
    total_received = 0;

    // Initialize SPI for FPGA (for later use)
    initSPI(0b111, 0, 0);
    pinMode(SPI_CE, GPIO_OUTPUT);
    digitalWrite(SPI_CE, 1);

    // Ready message
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Ready to Receive    ");
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Blocks: 0           ");
    
    // Blink LED to show ready
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, 1);
        delay_millis(TIM15, 100);
        digitalWrite(LED_PIN, 0);
        delay_millis(TIM15, 100);
    }

    // Main loop
    while (1) {
        // Process any received blocks from mechanical system
        process_received_blocks();
        
        // Service HTTP requests from ESP8266
        processWebRequest(USART);
        
        // Display debug info on LCD
        // Choose one of these display modes:
        
        // Option 1: Debug mode - shows raw ISR counts and byte values
        display_debug_info();
        
        // Option 2: Test mode - shows received bytes in hex
        // display_test_mode();
        
        // Option 3: Progress mode - shows transfer progress
        // lcd_update_transfer_status(&g_lcd, blocks_processed, 
        //                           get_total_expected_blocks());
        
        // Small delay to prevent overwhelming the LCD
        delay_millis(TIM15, 10);
    }
}