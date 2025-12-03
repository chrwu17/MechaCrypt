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

uint32_t keys_ready = 0;
/**
 * @brief Process received message blocks from mechanical system
 * NOW USES KEYS FROM SENDER MCU
 */
void process_received_blocks(void) {
    if (messageReceivedFlag) {
        messageReceivedFlag = 0;
        digitalWrite(LED_PIN, 1);
        
        // Check if we have the key for this block
        if (!keys_ready) {
            // Keys not received yet - skip or buffer
            digitalWrite(LED_PIN, 0);
            return;
        }
        
        // Fetch key for current block
        uint8_t key[16];
        int key_result = getReceivedKey(blocks_processed, key);
        
        if (key_result != 0) {
            // Key not available for this block index
            //led_error_blink();
            digitalWrite(LED_PIN, 0);
            return;
        }
        
        // Decrypt: KEY + CIPHERTEXT -> PLAINTEXT
        uint8_t plaintext[16];
        int decrypt_result = fpgaDecryptBlock(
            key,                               // Key from sender MCU
            (const uint8_t*)receivedMessage,   // Ciphertext from mechanical
            plaintext                          // Output plaintext
        );
        
        if (decrypt_result == 0) {
            // Decryption successful - store plaintext
            receiver_store_block(blocks_processed, plaintext);
            on_block_received(&g_lcd);
            blocks_processed++;
        } else {
            // Decryption failed
            //led_error_blink();
        }
        
        digitalWrite(LED_PIN, 0);
    }
}

/**
 * @brief Poll for incoming keys from sender MCU
 * Call this in main loop before processing blocks
 */
void poll_for_keys(void) {
    if (!keys_ready) {
        // Poll key reception state machine
        pollKeyReception();
        
        // Check if reception complete
        if (areKeysReceived()) {
            keys_ready = 1;
            
            // Update LCD with key count
            uint8_t num_keys = getNumKeysReceived();
            uint8_t orig_len = getOriginalMessageLength();
            
            lcd_set_cursor(&g_lcd, 0, 1);
            char buf[21];
            snprintf(buf, sizeof(buf), "Keys RX: %u (L=%u) ", num_keys, orig_len);
            lcd_print(&g_lcd, buf);
            
            // Set expected transfer size for progress bar
            set_expected_transfer_size(orig_len);
            
            // Visual feedback
            for (int i = 0; i < 3; i++) {
                digitalWrite(LED_PIN, 1);
                delay_millis(TIM15, 100);
                digitalWrite(LED_PIN, 0);
                delay_millis(TIM15, 100);
            }
        }
    }
}

/**
 * @brief Display status including key reception
 */
void display_status_with_keys(void) {
    static uint32_t last_update = 0;
    uint32_t now = get_millis();
    
    if (now - last_update < 500) return;
    last_update = now;
    
    char buf[21];
    
    // Line 0: Title
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "MechaCrypt Receiver ");
    
    // Line 1: Key status
    lcd_set_cursor(&g_lcd, 0, 1);
    if (keys_ready) {
        snprintf(buf, sizeof(buf), "Keys:%u Blk:%u    ", 
                 getNumKeysReceived(), blocks_processed);
    } else {
        lcd_print(&g_lcd, "Waiting for keys... ");
    }
    lcd_print(&g_lcd, buf);
    
    // Line 2 & 3: Progress bar (if keys received)
    if (keys_ready && get_total_expected_blocks() > 0) {
        lcd_update_transfer_status(&g_lcd, blocks_processed, 
                                    get_total_expected_blocks());
    }
}

// Main entry point
int main(void) {
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

    // Timers
    initTIM(TIM15);
    initTIM15_millis();

    // USART for ESP8266
    USART_TypeDef *USART = initUSART(USART1_ID, 125000);

    // LCD
    lcd_hw_init();
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "MechaCrypt Receiver ");
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Initializing...     ");
    delay_millis(TIM15, 1000);

    // Initialize mechanical message receiver
    initMsgReceive();

    // Initialize MCU-to-MCU SPI receiver (NEW!)
    initMCU_SPI_Receiver();

    // Clear storage
    for (int i = 0; i < MAX_BLOCKS; i++) {
        memset((void*)received_blocks[i], 0, 16);
        have_received[i] = 0;
    }
    total_received = 0;
    keys_ready = 0;

    // Initialize FPGA SPI
    initSPI(0b111, 0, 0);
    pinMode(SPI_CE, GPIO_OUTPUT);
    digitalWrite(SPI_CE, 1);
    
    // Initialize FPGA decrypt interface
    initFPGADecrypt();

    // Ready message
    lcd_clear(&g_lcd);
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "Waiting for Keys... ");
    
    // Blink to show ready
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, 1);
        delay_millis(TIM15, 100);
        digitalWrite(LED_PIN, 0);
        delay_millis(TIM15, 100);
    }

    // Main loop
    while (1) {
        // 1. Poll for incoming keys from sender MCU (PRIORITY)
        poll_for_keys();
        
        // 2. Process received blocks from mechanical system
        //    (only after keys are ready)
        process_received_blocks();
        
        // 3. Service HTTP requests
        processWebRequest(USART);
        
        // 4. Update display
        display_status_with_keys();
        
        // Small delay
        delay_millis(TIM15, 10);
    }
}