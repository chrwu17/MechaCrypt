/**
 * @file STM32L432KC_I2C.c
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Source code for I2C functions. 
 */

#include "../lib/STM32L432KC_I2C.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC_RCC.h"

void initI2C1(void) {
    // --- Enable clocks for GPIOA and I2C1 ---
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;

    // --- Configure PA9 and PA10 for I2C1 (AF4) ---

    // 1) Alternate function mode (10) on PA9/PA10
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));   // clear
    GPIOA->MODER |=  (2U << (9 * 2)) | (2U << (10 * 2));     // 10 = AF

    // 2) Open-drain outputs
    GPIOA->OTYPER |= (1U << 9) | (1U << 10);

    // 3) Pull-ups (assuming you also have *external* pull-ups on the bus)
    GPIOA->PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->PUPDR |=  (1U << (9 * 2)) | (1U << (10 * 2));     // 01 = pull-up

    // 4) Select AF4 for PA9/PA10 in AFR[1] (pins 8–15)
    GPIOA->AFR[1] &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  (4U   << ((9 - 8) * 4)) | (4U   << ((10 - 8) * 4)); // AF4

    // --- Basic I2C1 configuration (you can tweak TIMINGR if needed) ---

    I2C1->CR1 &= ~I2C_CR1_PE;   // Disable I2C1 before configuring

    // 100kHz timing for ~80 MHz system clock 
    I2C1->TIMINGR = 0x10909CEC;

    // Enable auto-end and ACK by default
    I2C1->CR2 = 0;

    // Enable I2C1 peripheral
    I2C1->CR1 |= I2C_CR1_PE;
}


int i2c_write_byte(uint8_t addr7, uint8_t data) {
    // Wait until not busy
    while (I2C1->ISR & I2C_ISR_BUSY);
    
    // Configure for 1 byte transfer
    I2C1->CR2 = 0;
    I2C1->CR2 |= (addr7 << 1);              // 7-bit address
    I2C1->CR2 |= (1 << I2C_CR2_NBYTES_Pos); // 1 byte
    I2C1->CR2 |= I2C_CR2_AUTOEND;           // Auto-generate STOP
    I2C1->CR2 |= I2C_CR2_START;             // Generate START
    
    // Wait for TXIS (transmit register empty)
    uint32_t timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && timeout--) {
        if (I2C1->ISR & I2C_ISR_NACKF) {
            I2C1->ICR |= I2C_ICR_NACKCF;
            return 1; // NACK received
        }
    }
    if (timeout == 0) return 1;
    
    // Send data
    I2C1->TXDR = data;
    
    // Wait for STOPF (transfer complete)
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && timeout--);
    if (timeout == 0) return 1;
    
    // Clear STOP flag
    I2C1->ICR |= I2C_ICR_STOPCF;
    
    return 0; // Success
}

int i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len) {
    if (len == 0 || len > 255) return 1;
    
    // Wait until not busy
    while (I2C1->ISR & I2C_ISR_BUSY);
    
    // Configure transfer
    I2C1->CR2 = 0;
    I2C1->CR2 |= (addr7 << 1);
    I2C1->CR2 |= (len << I2C_CR2_NBYTES_Pos);
    I2C1->CR2 |= I2C_CR2_AUTOEND;
    I2C1->CR2 |= I2C_CR2_START;
    
    // Send all bytes
    for (uint16_t i = 0; i < len; i++) {
        uint32_t timeout = 100000;
        while (!(I2C1->ISR & I2C_ISR_TXIS) && timeout--) {
            if (I2C1->ISR & I2C_ISR_NACKF) {
                I2C1->ICR |= I2C_ICR_NACKCF;
                return 1;
            }
        }
        if (timeout == 0) return 1;
        
        I2C1->TXDR = data[i];
    }
    
    // Wait for completion
    uint32_t timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && timeout--);
    if (timeout == 0) return 1;
    
    I2C1->ICR |= I2C_ICR_STOPCF;
    return 0;
}