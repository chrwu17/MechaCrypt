/**
 * @file STM32L432KC_SPI.c
 * @author Christian Wu
 * @date 2025-11-19
 * @brief Source code for SPI functions. Taken from the E155 Lecture
 */

#include "../lib/STM32L432KC.h"

void initSPI(int br, int cpol, int cpha) {

    RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN);
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // Set pins to AF
    pinMode(SPI_SCK,  GPIO_ALT); // PB3
    pinMode(SPI_CIPO, GPIO_ALT); // PB4
    pinMode(SPI_COPI, GPIO_ALT); // PB5
    pinMode(SPI_CE,   GPIO_OUTPUT);

    // High speed SCK
    GPIOB->OSPEEDR |= (3 << (3*2));

    // Clear AFR nibbles
    GPIOB->AFR[0] &= ~(0xF << (3*4));
    GPIOB->AFR[0] &= ~(0xF << (4*4));
    GPIOB->AFR[0] &= ~(0xF << (5*4));

    // Set AF5
    GPIOB->AFR[0] |= (5 << (3*4));
    GPIOB->AFR[0] |= (5 << (4*4));
    GPIOB->AFR[0] |= (5 << (5*4));

    SPI1->CR1 = _VAL2FLD(SPI_CR1_BR, br) | SPI_CR1_MSTR;
    SPI1->CR1 |= _VAL2FLD(SPI_CR1_CPOL, cpol) | _VAL2FLD(SPI_CR1_CPHA, cpha);
    SPI1->CR2 = _VAL2FLD(SPI_CR2_DS, 0b0111) | SPI_CR2_FRXTH | SPI_CR2_SSOE;

    SPI1->CR1 |= SPI_CR1_SPE;
}

char spiSendReceive(char send) {
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = send;
    while (!(SPI1->SR & SPI_SR_RXNE));
    return SPI1->DR;
}
