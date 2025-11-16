// top_spi_demo.sv
// Minimal top-level that exposes the SPI slave on I/O pins.

module top_spi_demo (
    input  logic reset_n,   // external reset (button or tied high)
    input  logic spi_cs_n,  // from MCU (chip select)
    input  logic spi_sck,   // from MCU
    input  logic spi_mosi,  // from MCU
    output logic spi_miso   // to MCU
);

    spi_block_demo_slave u_spi (
        .reset_n (reset_n),
        .spi_cs_n(spi_cs_n),
        .spi_sck (spi_sck),
        .spi_mosi(spi_mosi),
        .spi_miso(spi_miso)
    );

endmodule
