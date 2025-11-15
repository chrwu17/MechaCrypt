// Christian Wu
// chrwu@g.hmc.edu
// 09/30/25

#include "../lib/STM32L432KC_RCC.h"
#include "../lib/STM32L432KC_GPIO.h"

// These are your I2C pins — never modify them in pinMode()
#define I2C_SCL_PIN PA9
#define I2C_SDA_PIN PA10

void gpioEnable(int port_id) {
    switch (port_id) {
        case GPIO_PORT_A: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN; break;
        case GPIO_PORT_B: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; break;
        case GPIO_PORT_C: RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN; break;
    }
}

int gpioPinOffset(int gpio_pin) { return gpio_pin & 0x0F; }
int gpioPinToPort(int gpio_pin) { return gpio_pin >> 4; }

GPIO_TypeDef * gpioPortToBase(int port_id) {
    switch (port_id) {
        case GPIO_PORT_A: return GPIOA;
        case GPIO_PORT_B: return GPIOB;
        case GPIO_PORT_C: return GPIOC;
    }
    return 0;
}

GPIO_TypeDef * gpioPinToBase(int gpio_pin) {
    return gpioPortToBase(gpioPinToPort(gpio_pin));
}

void pinMode(int gpio_pin, int function) {

    // 🚨 Prevent ANY code from destroying I2C pins
    if (gpio_pin == I2C_SCL_PIN || gpio_pin == I2C_SDA_PIN)
        return;

    GPIO_TypeDef *GPIO = gpioPinToBase(gpio_pin);
    int pin = gpioPinOffset(gpio_pin);

    // Always clear BOTH bits of the 2-bit mode field
    GPIO->MODER &= ~(0b11 << (pin * 2));

    switch (function) {
        case GPIO_INPUT:
            // 00 (already cleared)
            break;

        case GPIO_OUTPUT:
            // 01
            GPIO->MODER |= (0b01 << (pin * 2));
            break;

        case GPIO_ALT:
            // 10
            GPIO->MODER |= (0b10 << (pin * 2));
            break;

        case GPIO_ANALOG:
            // 11
            GPIO->MODER |= (0b11 << (pin * 2));
            break;
    }
}

int digitalRead(int gpio_pin) {
    GPIO_TypeDef *GPIO = gpioPinToBase(gpio_pin);
    int pin = gpioPinOffset(gpio_pin);
    return (GPIO->IDR >> pin) & 1;
}

void digitalWrite(int gpio_pin, int val) {
    GPIO_TypeDef *GPIO = gpioPinToBase(gpio_pin);
    int pin = gpioPinOffset(gpio_pin);

    if (val) GPIO->ODR |= (1 << pin);
    else     GPIO->ODR &= ~(1 << pin);
}

void togglePin(int gpio_pin) {
    GPIO_TypeDef *GPIO = gpioPinToBase(gpio_pin);
    int pin = gpioPinOffset(gpio_pin);
    GPIO->ODR ^= (1 << pin);
}
