/*
    @author: Josaphat Ngoga (modified)
    @date:   11/16/2025
    @description: MCU code to communicate with FPGA 2 via shared SPI bus
                  Uses separate CS line to multiplex between two FPGAs.
*/

#include "../lib/STM32L432KC.h"
#include "../lib/bridge.h"

///////////////////////////////////////////////////////////////////////////////
// Bridge CS Initialization
///////////////////////////////////////////////////////////////////////////////

/* Initialize bridge chip select pin (SPI bus already configured) */
void initBridgeCS(void) {
    // Configure CS pin as output, start high (inactive)
    pinMode(BRIDGE_CS, GPIO_OUTPUT);
    digitalWrite(BRIDGE_CS, 1);  // CS idle high
    
    // Optional: configure additional control pins for FPGA 2
    pinMode(BRIDGE_LOAD, GPIO_OUTPUT);
    digitalWrite(BRIDGE_LOAD, 0);
    
    pinMode(BRIDGE_DONE, GPIO_INPUT);
}

///////////////////////////////////////////////////////////////////////////////
// Bridge Select/Deselect
///////////////////////////////////////////////////////////////////////////////

void bridgeSelect(void) {
    digitalWrite(BRIDGE_CS, 0);  // CS low = active
}

void bridgeDeselect(void) {
    digitalWrite(BRIDGE_CS, 1);  // CS high = inactive
}

///////////////////////////////////////////////////////////////////////////////
// Bridge Communication
///////////////////////////////////////////////////////////////////////////////

/* NOTE: Key transmission is now handled by bridge_send_all_keys() in webpage.c
 * which sends multiple keys sequentially using bridgeSelect/Deselect
 * and the shared spiSendReceive() function.
 * 
 * Protocol per key:
 * 1. bridgeSelect() - CS low
 * 2. Send 128 bits (16 bytes) MSB first, bit-by-bit via spiSendReceive()
 * 3. bridgeDeselect() - CS high
 * 4. Small inter-packet delay
 * 5. Repeat for each block's key
 */

///////////////////////////////////////////////////////////////////////////////
// Optional: Bridge handshake helpers
///////////////////////////////////////////////////////////////////////////////

/* Assert LOAD signal to FPGA 2 */
void bridgeAssertLoad(void) {
    digitalWrite(BRIDGE_LOAD, 1);
}

/* Deassert LOAD signal to FPGA 2 */
void bridgeDeassertLoad(void) {
    digitalWrite(BRIDGE_LOAD, 0);
}

/* Check if FPGA 2 DONE signal is high */
int bridgeIsDone(void) {
    return digitalRead(BRIDGE_DONE);
}