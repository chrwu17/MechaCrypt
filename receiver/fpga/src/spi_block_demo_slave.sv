// spi_block_demo_slave.sv
// Simple SPI slave that serves 128-bit (16-byte) blocks to an MCU master.
// SPI mode 0: CPOL=0, CPHA=0 (sample on rising edge).

module spi_block_demo_slave (
    input  logic reset_n,    // active-low reset
    input  logic spi_cs_n,   // chip select from MCU (active low)
    input  logic spi_sck,    // SPI clock from MCU
    input  logic spi_mosi,   // MOSI from MCU (ignored, but captured)
    output logic spi_miso    // MISO to MCU
);

    // 128-bit shift register for outgoing/incoming data
    logic [127:0] shift_reg;
    logic [7:0]   bit_cnt;    // counts bits in current transfer (0..127)
    logic [1:0]   block_idx;  // which demo block we are on (0..3)

    // Demo blocks (16 bytes each) – tweak these however you like.

    // "Hello, World!\n\0\0"
    localparam logic [127:0] BLOCK0 =
        128'h48656C6C6F2C20576F726C64210A0000;

    // "MechaCrypt RX    "
    localparam logic [127:0] BLOCK1 =
        128'h4D656368614372797074205258202020;

    // "SPI Demo Block 2!"
    localparam logic [127:0] BLOCK2 =
        128'h5350492044656D6F20426C6F636B203221;

    // "Last demo block."
    localparam logic [127:0] BLOCK3 =
        128'h4C6173742064656D6F20626C6F636B2E;

    logic [127:0] cur_block;

    // Choose which block to load based on block_idx
    always_comb begin
        unique case (block_idx)
            2'd0:    cur_block = BLOCK0;
            2'd1:    cur_block = BLOCK1;
            2'd2:    cur_block = BLOCK2;
            default: cur_block = BLOCK3;
        endcase
    end

    // Single clocked process:
    //  - Clock domain = SPI SCK (master provides it)
    //  - Async reset on reset_n
    //  - CS handled inside via spi_cs_n
    always_ff @(posedge spi_sck or negedge reset_n) begin
        if (!reset_n) begin
            bit_cnt   <= 8'd0;
            block_idx <= 2'd0;
            shift_reg <= BLOCK0;
        end else begin
            if (spi_cs_n) begin
                // Not selected: idle. Reset counter so next transfer
                // starts fresh on next active CS.
                bit_cnt <= 8'd0;
            end else begin
                // Selected: active transfer
                if (bit_cnt == 8'd0) begin
                    // On first bit of a new transfer, load the current block
                    shift_reg <= cur_block;
                end else begin
                    // Shift out MSB, shift in new bit on LSB
                    shift_reg <= {shift_reg[126:0], spi_mosi};
                end

                // Count bits 0..127
                if (bit_cnt == 8'd127) begin
                    bit_cnt   <= 8'd0;           // wrap for next transfer
                    block_idx <= block_idx + 2'd1;  // next demo block
                end else begin
                    bit_cnt <= bit_cnt + 8'd1;
                end
            end
        end
    end

    // Always drive the MSB of shift_reg on MISO
    assign spi_miso = shift_reg[127];

endmodule
