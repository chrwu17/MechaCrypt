/*
 * Receiver main.c
 *
 * Mode: decrypt blocks from FPGA, expose them over HTTP,
 *       and show basic status on the LCD.
 *
 * - USART1 goes out on PB6 (TX) / PB7 (RX) to ESP8266.
 * - PA9 / PA10 are used ONLY for bit-banged I2C to the LCD.
 */

#include "../lib/main.h"
#include "../lib/webpage.h"
#include "../lib/STM32L432KC.h"
#include "../lib/STM32L432KC_TIM.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"

// Adjust these includes to match your actual LCD/I2C header names:
#include "../lib/lcd_i2c.h"
#include "../lib/i2c_bitbang.h"

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

// ----------------- LCD + bit-banged I2C init on PA9/PA10 -----------------
static void lcd_hw_init(void)
{
    // Make sure GPIOA clock is on
    gpioEnable(GPIO_PORT_A);

    // Configure PA9/PA10 as open-drain outputs with pull-ups, idle high.
    // This matches i2c_bitbang.c (SCL=9, SDA=10) and keeps them out of USART AF.
    GPIOA->MODER &= ~((3u << (9*2)) | (3u << (10*2)));  // clear mode
    GPIOA->MODER |=  ((1u << (9*2)) | (1u << (10*2)));  // output mode
    GPIOA->OTYPER |= (1u << 9) | (1u << 10);            // open-drain
    GPIOA->PUPDR &= ~((3u << (9*2)) | (3u << (10*2)));  // clear PU/PD
    GPIOA->PUPDR |=  ((1u << (9*2)) | (1u << (10*2)));  // pull-up
    GPIOA->ODR   |=  (1u << 9) | (1u << 10);            // idle high

    // Basic LCD init sequence using 0x27 backpack
    lcd_init(&lcd, 0x27, 20, 4, i2c_bitbang_write, delay_us);
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

    // Bring up USART1 at 125000 baud (match your ESP8266 config)
    USART_TypeDef *USART = initUSART(USART1_ID, 125000);

    // --- Remap USART1 to PB6 (TX) / PB7 (RX) ---

    // Configure PB6/PB7 as alternate function pins
    pinMode(PB6, GPIO_ALT);
    pinMode(PB7, GPIO_ALT);

    // Clear existing AF bits for pins 6 and 7
    GPIOB->AFR[0] &= ~((0xF << GPIO_AFRL_AFSEL6_Pos) |
                       (0xF << GPIO_AFRL_AFSEL7_Pos));

    // Set AF7 (USART1) for PB6 and PB7
    GPIOB->AFR[0] |= (0b0111 << GPIO_AFRL_AFSEL6_Pos) |
                     (0b0111 << GPIO_AFRL_AFSEL7_Pos);

    // Now reclaim PA9/PA10 for bit-banged I2C to the LCD
    lcd_hw_init();

    // Main loop:
    //  - service HTTP
    //  - (your SPI receiver logic will call receiver_store_block())
    while (1) {
        processWebRequest(USART);

        // TODO: if you want, you can update the LCD here:
        //  - e.g., show total_received or first N chars of plaintext.
        //
        // Example (pseudo-code):
        // lcd_set_cursor(&lcd, 0, 1);
        // char buf[21];
        // snprintf(buf, sizeof(buf), "Blocks: %u", total_received);
        // lcd_print(&lcd, buf);
    }
}
