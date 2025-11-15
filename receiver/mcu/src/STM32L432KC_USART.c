// STM32L432KC_USART.c

#include "../lib/STM32L432KC.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC_RCC.h"

USART_TypeDef * id2Port(int USART_ID) {
    switch (USART_ID) {
        case USART1_ID: return USART1;
        case USART2_ID: return USART2;
        default: return 0;
    }
}

USART_TypeDef * initUSART(int USART_ID, int baud_rate) {

    gpioEnable(GPIO_PORT_A);
    gpioEnable(GPIO_PORT_B);

    RCC->CR |= RCC_CR_HSION;

    USART_TypeDef *USART = id2Port(USART_ID);

    switch (USART_ID) {

        case USART1_ID:
            RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
            RCC->CCIPR |= (0b10 << RCC_CCIPR_USART1SEL_Pos); // HS16

            // =======================================================
            //   REMAP USART1 to PB6 = TX, PB7 = RX (AF7)
            // =======================================================
            pinMode(PB6, GPIO_ALT);
            pinMode(PB7, GPIO_ALT);

            // Clear AFR entries
            GPIOB->AFR[0] &= ~(0xF << (6*4)); 
            GPIOB->AFR[0] &= ~(0xF << (7*4));

            // Set AF7
            GPIOB->AFR[0] |= (7 << (6*4)); 
            GPIOB->AFR[0] |= (7 << (7*4));

            // High speed for clean UART edges
            GPIOB->OSPEEDR |= (3 << (6*2));
            GPIOB->OSPEEDR |= (3 << (7*2));
            break;

        case USART2_ID:
            RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
            RCC->CCIPR |= (0b10 << RCC_CCIPR_USART2SEL_Pos);

            pinMode(PA2, GPIO_ALT);
            pinMode(PA15, GPIO_ALT);

            GPIOA->AFR[0] |= (7 << GPIO_AFRL_AFSEL2_Pos);
            GPIOA->AFR[1] |= (3 << GPIO_AFRH_AFSEL15_Pos);
            break;
    }

    USART->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1);
    USART->CR1 &= ~USART_CR1_OVER8;
    USART->CR2 &= ~USART_CR2_STOP;

    USART->BRR = (uint16_t)(HSI_FREQ / baud_rate);

    USART->CR1 |= USART_CR1_UE;
    USART->CR1 |= USART_CR1_TE | USART_CR1_RE;

    return USART;
}

void sendChar(USART_TypeDef *USART, char data) {
    while (!(USART->ISR & USART_ISR_TXE));
    USART->TDR = data;
    while (!(USART->ISR & USART_ISR_TC));
}

void sendString(USART_TypeDef *USART, char *str) {
    for (uint32_t i = 0; str[i] != 0; i++)
        sendChar(USART, str[i]);
}

char readChar(USART_TypeDef *USART) {
    return USART->RDR;
}

void readString(USART_TypeDef *USART, char *dst) {
    int i = 0;
    do {
        dst[i++] = readChar(USART);
    } while (USART->ISR & USART_ISR_RXNE);
}
