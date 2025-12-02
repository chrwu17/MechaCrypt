/**
 * @file main.c
 * @author Christian Wu
 * @date 2025-11-25
 * @brief Main file for MechaCrypt receiver MCU firmware.
 * 
 * SYSTEM FLOW:
 * 1. Bridge FPGA receives key + message length from sender MCU via SPI
 * 2. Mechanical system sends ciphertext bytes (triggers ISR on MCU)
 * 3. After 16 bytes received, main loop detects block ready
 * 4. Main loop sends ciphertext to FPGA via SPI
 * 5. FPGA decrypts using key it already has
 * 6. FPGA returns plaintext via SPI
 * 7. Main loop stores plaintext for webpage display
 * 8. LCD progress bar updates
 * 9. Repeat for next block
 */

#include "../lib/main.h"
#include "../lib/fpga_decrypt.h"

// ----------------- LCD handle (global for msgReceive) -----------------
lcd_i2c_t g_lcd;

// ----------------- Microsecond delay using SysTick -----------------
void delay_us(uint32_t us)
{
    if (us == 0) return;
    uint32_t ticks = (SystemCoreClock / 1000000u) * us;
    if (ticks > 0xFFFFFFu) ticks = 0xFFFFFFu;
    SysTick->LOAD = ticks;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0);
    SysTick->CTRL = 0;
}

// ----------------- LCD initialization -----------------
static void lcd_hw_init(void)
{
    initI2C1();
    lcd_init(&g_lcd, 0x27, 20, 4, i2c_write_byte, delay_us);
    lcd_begin(&g_lcd);
    lcd_backlight(&g_lcd);
    lcd_init_progress_chars(&g_lcd);
    lcd_clear(&g_lcd);
    lcd_update_transfer_status(&g_lcd, 0, 0);
}

// ----------------- Process one ciphertext block -----------------
static void processBlock(void)
{
    uint8_t ciphertext[16];
    uint8_t plaintext[16];
    
    // Check if we've exceeded storage capacity
    uint16_t block_idx = getMsgReceivedBlockCount();
    if (block_idx >= 64) {  // MAX_BLOCKS
        lcd_set_cursor(&g_lcd, 0, 3);
        lcd_print(&g_lcd, "Storage Full!       ");
        clearBlockReadyFlag();
        return;
    }
    
    // Get ciphertext from mechanical receiver
    getCurrentCiphertextBlock(ciphertext);
    
    // Visual feedback - blink LED
    digitalWrite(LED_PIN, 1);
    
    // Send to FPGA for decryption (FPGA already has the key)
    int result = fpgaDecryptBlock(ciphertext, plaintext);
    
    if (result == 0) {
        // Decryption successful!
        
        // Store plaintext for webpage
        receiver_store_block(block_idx, plaintext);
        
        // Update counters and LCD
        incrementBlockCount();
        
    } else {
        // Decryption timeout/error
        lcd_set_cursor(&g_lcd, 0, 3);
        lcd_print(&g_lcd, "FPGA Timeout!       ");
    }
    
    // Clear LED and ready flag
    digitalWrite(LED_PIN, 0);
    clearBlockReadyFlag();
}

// ----------------- Main entry -----------------
int main(void)
{
    // ==== SYSTEM INITIALIZATION ====
    
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

    // USART1 for ESP8266 web interface (PB6=TX, PB7=RX at 125000 baud)
    USART_TypeDef *USART = initUSART(USART1_ID, 125000);

    // LCD display
    lcd_hw_init();

    // Initialize message receiver (mechanical system → MCU)
    initMsgReceive();

    // Initialize SPI for FPGA communication
    initSPI(0b111, 0, 0);  // Slowest speed for reliability
    pinMode(SPI_CE, GPIO_OUTPUT);
    digitalWrite(SPI_CE, 1);  // CS idle high

    // Initialize FPGA decryption interface
    // Note: FPGA already has the key from bridge module
    initFPGADecrypt();

    // ==== DISPLAY READY MESSAGE ====
    
    lcd_set_cursor(&g_lcd, 0, 0);
    lcd_print(&g_lcd, "MechaCrypt Ready    ");
    lcd_set_cursor(&g_lcd, 0, 1);
    lcd_print(&g_lcd, "Waiting for data... ");

    // ==== OPTIONAL: Load demo data for testing ====
    // Uncomment to test webpage without mechanical system:
    // receiver_demo_init_plaintext();

    // ==== MAIN LOOP ====
    
    while (1) {
        // 1. Check if a ciphertext block is ready from mechanical system
        if (isBlockReady()) {
            processBlock();
        }

        // 2. Service web requests from ESP8266
        processWebRequest(USART);

        // 3. Check if transfer is complete
        uint16_t received = get_received_block_count();
        uint16_t expected = get_total_expected_blocks();
        
        if (expected > 0 && received >= expected) {
            // All blocks received and decrypted!
            lcd_set_cursor(&g_lcd, 0, 3);
            lcd_print(&g_lcd, "Transfer Complete!  ");
            digitalWrite(LED_PIN, 1);  // Keep LED on
        }

        // 4. Small delay to prevent busy-waiting
        delay_millis(TIM15, 10);
    }
}