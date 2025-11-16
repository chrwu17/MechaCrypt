// Christian Wu
// chrwu@g.hmc.edu
// 09/30/25

// Taken from the E155 Course Website

// STM32L432KC_USART.c
// Source code for USART functions

#include "../lib/STM32L432KC.h"
#include "../lib/STM32L432KC_USART.h"
#include "../lib/STM32L432KC_GPIO.h"
#include "../lib/STM32L432KC_RCC.h"

USART_TypeDef * id2Port(int USART_ID) {
    USART_TypeDef * USART;
    switch(USART_ID){
        case(USART1_ID) :
            USART = USART1;
            break;
        case(USART2_ID) :
            USART = USART2;
            break;
        default :
            USART = 0;
    }
    return USART;
}

// In STM32L432KC_USART.c

USART_TypeDef * initUSART(int USART_ID, int baud_rate) {
    // Enable GPIO clocks
    gpioEnable(GPIO_PORT_A);
    gpioEnable(GPIO_PORT_B);

    // Turn on HSI16 for USART clock
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}

    USART_TypeDef *USART = id2Port(USART_ID);

    switch (USART_ID) {
        case USART1_ID: {
            // Enable USART1 clock
            RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

            // Select HSI16 as USART1 clock source (USART1SEL = 10b)
            RCC->CCIPR &= ~RCC_CCIPR_USART1SEL_Msk;
            RCC->CCIPR |=  (0b10 << RCC_CCIPR_USART1SEL_Pos);

            // PB6 = TX, PB7 = RX, AF7 for USART1
            pinMode(PB6, GPIO_ALT);
            pinMode(PB7, GPIO_ALT);

            // Clear then set AFRL bits for PB6/PB7 to AF7
            GPIOB->AFR[0] &= ~((0xF << GPIO_AFRL_AFSEL6_Pos) |
                               (0xF << GPIO_AFRL_AFSEL7_Pos));
            GPIOB->AFR[0] |=  (0x7 << GPIO_AFRL_AFSEL6_Pos) |
                              (0x7 << GPIO_AFRL_AFSEL7_Pos);
            break;
        }

        case USART2_ID: {
            // (If/when you use USART2, configure here similarly)
            RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
            RCC->CCIPR &= ~RCC_CCIPR_USART2SEL_Msk;
            RCC->CCIPR |=  (0b10 << RCC_CCIPR_USART2SEL_Pos); // HSI16

            pinMode(PA2,  GPIO_ALT); // TX
            pinMode(PA15, GPIO_ALT); // RX

            GPIOA->AFR[0] &= ~(0xF << GPIO_AFRL_AFSEL2_Pos);
            GPIOA->AFR[0] |=  (0x7 << GPIO_AFRL_AFSEL2_Pos);   // AF7 on PA2

            GPIOA->AFR[1] &= ~(0xF << GPIO_AFRH_AFSEL15_Pos);
            GPIOA->AFR[1] |=  (0x3 << GPIO_AFRH_AFSEL15_Pos);  // AF3 on PA15
            break;
        }

        default:
            return 0;
    }

    // 8N1, oversampling by 16
    USART->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1);
    USART->CR1 &= ~USART_CR1_OVER8;
    USART->CR2 &= ~USART_CR2_STOP;

    // Baud: HSI16 (16 MHz) / baud_rate (125000)
    USART->BRR = (uint16_t)(HSI_FREQ / baud_rate);  // HSI_FREQ = 16000000

    USART->CR1 |= USART_CR1_UE;                 // Enable USART
    USART->CR1 |= USART_CR1_TE | USART_CR1_RE;  // Enable TX & RX

    return USART;
}




void sendChar(USART_TypeDef * USART, char data){
    while(!(USART->ISR & USART_ISR_TXE));
    USART->TDR = data;
    while(!(USART->ISR & USART_ISR_TC));
}

void sendString(USART_TypeDef * USART, char * charArray){

    uint32_t i = 0;
    do{
        sendChar(USART, charArray[i]);
        i++;
    }
    while(charArray[i] != 0);
}

char readChar(USART_TypeDef * USART) {
        char data = USART->RDR;
        return data;
}

void readString(USART_TypeDef * USART, char* charArray){
    int i = 0;
    do{
        charArray[i] = readChar(USART);
        i++;
    }
    while(USART->ISR & USART_ISR_RXNE);
}