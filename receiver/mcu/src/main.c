// main.c — FULL DIAGNOSTIC VERSION
// Finds exactly who overwrites PA9/PA10 (I2C pins)

#include "stm32l4xx.h"
#include "../lib/lcd_i2c.h"
#include "../lib/i2c_bitbang.h"
#include "../lib/main.h"
#include "../lib/STM32L432KC.h"
#include "../lib/webpage.h"

// ===========================================================
// GLOBAL DEBUG WATCH VARIABLES (VISIBLE IN SEGGER)
// ===========================================================

// I2C ACK results
volatile int lcd_ack_27 = -999;
volatile int lcd_ack_3F = -999;

// GPIOA register snapshots
volatile uint32_t dbg_MODER_A  = 0;
volatile uint32_t dbg_OTYPER_A = 0;
volatile uint32_t dbg_PUPDR_A  = 0;

volatile uint32_t dbg_MODER_A2  = 0;
volatile uint32_t dbg_OTYPER_A2 = 0;
volatile uint32_t dbg_PUPDR_A2  = 0;

volatile uint32_t dbg_MODER_A3  = 0;
volatile uint32_t dbg_OTYPER_A3 = 0;
volatile uint32_t dbg_PUPDR_A3  = 0;

volatile uint32_t dbg_MODER_A4  = 0;
volatile uint32_t dbg_OTYPER_A4 = 0;
volatile uint32_t dbg_PUPDR_A4  = 0;

// ===========================================================
// Microsecond Delay (SysTick)
// ===========================================================
void delay_us(uint32_t us)
{
    if (us == 0) return;

    uint32_t ticks = (SystemCoreClock / 1000000) * us;
    if (ticks > 0xFFFFFF) ticks = 0xFFFFFF;

    SysTick->LOAD = ticks;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL = 0;
}

// ===========================================================
// Clock Init — HSI16
// ===========================================================
static void Clock_Init_HSI16(void)
{
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_HSI;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);

    SystemCoreClockUpdate();
}

// ===========================================================
// MAIN
// ===========================================================
int main(void) {

    // STEP 0 — System init
    configureFlash();
    configureClock();
    Clock_Init_HSI16();
    SystemCoreClockUpdate();

    // STEP 1 — Enable GPIOA
    gpioEnable(GPIO_PORT_A);

    // STEP 2 — Configure PA9/PA10 FOR BITBANG I2C
    GPIOA->MODER &= ~((3u<<(9*2)) | (3u<<(10*2)));  // clear mode
    GPIOA->MODER |=  ((1u<<(9*2)) | (1u<<(10*2)));  // output mode
    GPIOA->OTYPER |= (1u<<9) | (1u<<10);            // open-drain
    GPIOA->PUPDR &= ~((3u<<(9*2)) | (3u<<(10*2)));  // clear PU/PD
    GPIOA->PUPDR |=  ((1u<<(9*2)) | (1u<<(10*2)));  // pull-up
    GPIOA->ODR   |=  (1u<<9) | (1u<<10);            // idle high

    // SNAPSHOT #1 — AFTER I2C CONFIG
    dbg_MODER_A  = GPIOA->MODER;
    dbg_OTYPER_A = GPIOA->OTYPER;
    dbg_PUPDR_A  = GPIOA->PUPDR;

    // STEP 3 — RAW I2C ADDRESS PROBE (BEFORE LCD INIT)
    lcd_ack_27 = i2c_bitbang_write(0x27, 0x00);
    lcd_ack_3F = i2c_bitbang_write(0x3F, 0x00);

    // SNAPSHOT #2 — AFTER PROBING
    dbg_MODER_A2  = GPIOA->MODER;
    dbg_OTYPER_A2 = GPIOA->OTYPER;
    dbg_PUPDR_A2  = GPIOA->PUPDR;

    // STEP 4 — LCD INIT
    lcd_i2c_t lcd;
    lcd_init(&lcd, 0x27, 20, 4, i2c_bitbang_write, delay_us);
    lcd_begin(&lcd);
    lcd_backlight(&lcd);
    lcd_set_cursor(&lcd, 0, 0);
    lcd_print(&lcd, "LCD DEBUG TEST 1");

    // SNAPSHOT #3 — AFTER LCD INIT
    dbg_MODER_A3  = GPIOA->MODER;
    dbg_OTYPER_A3 = GPIOA->OTYPER;
    dbg_PUPDR_A3  = GPIOA->PUPDR;

    // STEP 5 — NOW init the rest (SPI, USART, webpage)
    gpioEnable(GPIO_PORT_B);
    gpioEnable(GPIO_PORT_C);
    initTIM(TIM15);

    USART_TypeDef *USART = initUSART(USART1_ID, 125000);
    web_init();

    // SNAPSHOT #4 — AFTER ALL INITIALIZATION
    dbg_MODER_A4  = GPIOA->MODER;
    dbg_OTYPER_A4 = GPIOA->OTYPER;
    dbg_PUPDR_A4  = GPIOA->PUPDR;

    // LOOP (keep system active)
    while (1) {
        web_poll_uart(USART);
        web_poll_spi();
    }
}
