#include "stm32l4xx.h"
#include "../lib/i2c_bitbang.h"
#include "../lib/lcd_i2c.h"
#include "../lib/STM32L432KC.h"

#define SCL_PIN 9
#define SDA_PIN 10

static inline void bb_delay(void){
    volatile int i;
    for(i = 0; i < 450; i++) __asm__("nop");
}

static inline void SDA_out(void){
    GPIOA->MODER &= ~(3u << (SDA_PIN*2));
    GPIOA->MODER |=  (1u << (SDA_PIN*2));
}
static inline void SDA_in(void){
    GPIOA->MODER &= ~(3u << (SDA_PIN*2));
}
static inline void SCL_out(void){
    GPIOA->MODER &= ~(3u << (SCL_PIN*2));
    GPIOA->MODER |=  (1u << (SCL_PIN*2));
}
static inline void SCL_in(void){
    GPIOA->MODER &= ~(3u << (SCL_PIN*2));
}

static inline void SDA_low(void){
    SDA_out();
    GPIOA->ODR &= ~(1u << SDA_PIN);
}
static inline void SDA_rel(void){
    SDA_in();
}
static inline void SCL_low(void){
    SCL_out();
    GPIOA->ODR &= ~(1u << SCL_PIN);
}
static inline void SCL_rel(void){
    SCL_in();
}

// --- I2C Primitives ---
static void i2c_start(void){
    SDA_rel();
    SCL_rel();
    bb_delay();
    SDA_low();
    bb_delay();
    SCL_low();
}

static void i2c_stop(void){
    SDA_low();
    bb_delay();
    SCL_rel();
    bb_delay();
    SDA_rel();
    bb_delay();
}

static uint8_t i2c_write(uint8_t b){
    for(int i=0;i<8;i++){
        if(b & 0x80) SDA_rel();
        else SDA_low();

        bb_delay();
        SCL_rel();
        bb_delay();
        SCL_low();

        b <<= 1;
    }

    SDA_rel();
    bb_delay();
    SCL_rel();
    bb_delay();

    uint8_t ack = (GPIOA->IDR >> SDA_PIN) & 1;

    SCL_low();
    return ack;
}

// Public API for LCD driver
int i2c_bitbang_write(uint8_t addr7, uint8_t data){
    uint8_t addr = addr7 << 1;

    i2c_start();
    if(i2c_write(addr)){ i2c_stop(); return -1; }
    if(i2c_write(data)){ i2c_stop(); return -2; }
    i2c_stop();
    return 0;
}
