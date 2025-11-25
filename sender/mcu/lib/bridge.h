/*
    Header file for the bridge module
    Implements SPI communication with a second FPGA using shared SPI bus
    but separate chip select line.
*/

#ifndef BRIDGE_H
#define BRIDGE_H

#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// Pin Definitions for Shared SPI Bus
///////////////////////////////////////////////////////////////////////////////

// Shared SPI signals (same as main FPGA)
#define BRIDGE_SCK    PB3   // Shared SPI clock
#define BRIDGE_COPI   PB5   // Shared SPI MOSI
#define BRIDGE_CIPO   PB4   // Shared SPI MISO (if needed)

// Dedicated chip select for second FPGA
#define BRIDGE_CS     PA8   // Different CS from main FPGA (PA11)

// Optional control signals for second FPGA
#define BRIDGE_LOAD   PA7   // Load signal for FPGA 2 (if needed)
#define BRIDGE_DONE   PA12  // Done signal from FPGA 2 (if needed)

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes
///////////////////////////////////////////////////////////////////////////////

/* Initialize the bridge CS pin (SPI already initialized by main code)
 * Call this AFTER initSPI() has been called for the main FPGA */
void initBridgeCS(void);

/* Select FPGA 2 by asserting its chip select (CS low) */
void bridgeSelect(void);

/* Deselect FPGA 2 by deasserting its chip select (CS high) */
void bridgeDeselect(void);

/* Send 16-byte key and 1-byte length to FPGA 2
 *    -- key: pointer to 16-byte encryption key
 *    -- length: message length byte
 * This uses the SHARED spiSendReceive() from main SPI driver */
void bridgeSendKeyAndLength(const uint8_t *key, uint8_t length);

#endif // BRIDGE_H