// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

// SPI Transmitter Module
// Sends 128-bit plaintext to MCU via SPI

module spi_transmitter(
    input  logic          clk,
    input  logic          reset,
    input  logic [127:0]  plaintext,
    input  logic          load,         // Start transmission
    
    // SPI interface
    input  logic          sck,
    input  logic          cs,
    output logic          sdo,
    
    output logic          ready,        // Ready to transmit
    output logic          busy);        // Transmitting

    // Synchronize sck
    logic sck_sync_0, sck_sync_1, sck_prev;
    logic sck_fall;
    
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            sck_sync_0 <= 1'b0;
            sck_sync_1 <= 1'b0;
            sck_prev   <= 1'b0;
        end else begin
            sck_sync_0 <= sck;
            sck_sync_1 <= sck_sync_0;
            sck_prev   <= sck_sync_1;
        end
    end
    
    assign sck_fall = !sck_sync_1 && sck_prev;

    // Synchronize cs
    logic cs_sync_0, cs_sync_1, cs_prev;
    logic cs_rise;
    
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            cs_sync_0 <= 1'b1;
            cs_sync_1 <= 1'b1;
            cs_prev   <= 1'b1;
        end else begin
            cs_sync_0 <= cs;
            cs_sync_1 <= cs_sync_0;
            cs_prev   <= cs_sync_1;
        end
    end
    
    assign cs_rise = cs_sync_1 && !cs_prev;

    // SPI shift register
    logic [127:0] shift_reg;
    logic sdo_next;
    logic transmitting;
    
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            shift_reg    <= 128'd0;
            sdo_next     <= 1'b0;
            transmitting <= 1'b0;
        end 
        else begin
            // Load plaintext when requested
            if (load && !transmitting) begin
                shift_reg    <= plaintext;
                sdo_next     <= plaintext[127];
                transmitting <= 1'b1;
            end
            
            // End transmission on CS rise
            if (cs_rise && transmitting) begin
                transmitting <= 1'b0;
                sdo_next     <= 1'b0;
            end
            
            // Shift data on SCK falling edge while CS is low
            if (sck_fall && transmitting && !cs_sync_1) begin
                shift_reg <= {shift_reg[126:0], 1'b0};
                sdo_next  <= shift_reg[126];
            end
        end
    end

    assign sdo   = sdo_next;
    assign busy  = transmitting;
    assign ready = !transmitting;

endmodule