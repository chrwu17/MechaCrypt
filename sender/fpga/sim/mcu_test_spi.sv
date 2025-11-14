module mcu_test_spi (
    input  logic       reset,         // Active high reset
    
    // SPI signals
    input  logic       sck,           // SPI clock from MCU (PB3)
    input  logic       sdi,           // SPI data from MCU (PB5 - COPI)
    input  logic       cs_n,          // Chip select from MCU (PA11 - inverted)
    input  logic       load,          // Load signal from MCU (PA5)
    
    // Test outputs
    output logic       done,          // Done signal to MCU (PA6)
    output logic       led_block0_pt, // LED: Block 0 plaintext match
    output logic       led_block1_pt, // LED: Block 1 plaintext match
    output logic       led_debug      // DEBUG: blinks on any activity
);

    // Expected plaintext for block 0: "This is a test r"
    // Byte-reversed because MCU sends pt16[0] first
    localparam logic [127:0] EXPECTED_BLOCK0 = {
        8'h72, 8'h20, 8'h74, 8'h73, 8'h65, 8'h74, 8'h20, 8'h61,
        8'h20, 8'h73, 8'h69, 8'h20, 8'h73, 8'h69, 8'h68, 8'h54
    };
    
    // Expected plaintext for block 1: "un" + padding (0E repeated)
    // Byte-reversed
    localparam logic [127:0] EXPECTED_BLOCK1 = {
        8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h0E,
        8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h0E, 8'h6E, 8'h75
    };

    // Shift registers matching MCU protocol
    logic [127:0] key, plaintext;
    logic [8:0]   bit_counter;
    logic [1:0]   block_num;
    
    // Debug - light LED whenever we see activity
    logic [15:0] activity_counter;
    
    // State machine
    typedef enum logic [2:0] {
        IDLE         = 3'd0,
        WAIT_CS_LOW  = 3'd1,
        SHIFTING     = 3'd2,
        CHECK_DATA   = 3'd3,
        DONE_HIGH    = 3'd4,
        WAIT_LOAD_LOW= 3'd5
    } state_t;
    
    state_t state;
    
    // Edge detection registers
    logic load_prev, cs_n_prev;
    
    // Main state machine and shift register - all on sck domain
    always_ff @(posedge sck or posedge reset) begin
        if (reset) begin
            state <= IDLE;
            plaintext <= 128'b0;
            key <= 128'b0;
            bit_counter <= 9'd0;
            block_num <= 2'd0;
            done <= 1'b0;
            led_block0_pt <= 1'b0;
            led_block1_pt <= 1'b0;
            load_prev <= 1'b0;
            cs_n_prev <= 1'b1;
            activity_counter <= 16'd0;
            led_debug <= 1'b0;
        end else begin
            // Track edges
            load_prev <= load;
            cs_n_prev <= cs_n;
            
            // Activity indicator - toggle debug LED periodically
            activity_counter <= activity_counter + 1'd1;
            if (activity_counter == 16'd0) begin
                led_debug <= ~led_debug;
            end
            
            case (state)
                IDLE: begin
                    done <= 1'b0;
                    bit_counter <= 9'd0;
                    
                    // Detect load rising edge
                    if (load && !load_prev) begin
                        state <= WAIT_CS_LOW;
                        led_debug <= 1'b1; // Show we saw LOAD
                    end
                end
                
                WAIT_CS_LOW: begin
                    // Wait for CS to go low (start of SPI)
                    if (!cs_n && cs_n_prev) begin
                        state <= SHIFTING;
                        bit_counter <= 9'd0;
                    end
                    // If load goes low, abort
                    if (!load && load_prev) begin
                        state <= IDLE;
                    end
                end
                
                SHIFTING: begin
                    // Shift in data (MSB first)
                    // After each shift: sdi goes into key[0], key shifts up into plaintext
                    {plaintext, key} <= {plaintext[126:0], key, sdi};
                    bit_counter <= bit_counter + 1'd1;
                    
                    // After 256 bits, move to check
                    if (bit_counter == 9'd255) begin
                        state <= CHECK_DATA;
                    end
                    
                    // If CS goes high early (shouldn't happen), go to check anyway
                    if (cs_n && !cs_n_prev && bit_counter >= 9'd128) begin
                        state <= CHECK_DATA;
                    end
                end
                
                CHECK_DATA: begin
                    // Validate received data
                    if (block_num == 2'd0) begin
                        // Check block 0
                        if (plaintext == EXPECTED_BLOCK0 && key != 128'd0)
                            led_block0_pt <= 1'b1;
                    end 
                    else if (block_num == 2'd1) begin
                        // Check block 1
                        if (plaintext == EXPECTED_BLOCK1 && key != 128'd0)
                            led_block1_pt <= 1'b1;
                    end
                    
                    state <= DONE_HIGH;
                end
                
                DONE_HIGH: begin
                    // Assert DONE to tell MCU we're ready
                    done <= 1'b1;
                    state <= WAIT_LOAD_LOW;
                end
                
                WAIT_LOAD_LOW: begin
                    // Wait for MCU to acknowledge by lowering LOAD
                    if (!load && load_prev) begin
                        done <= 1'b0;
                        block_num <= block_num + 1'd1;
                        
                        // If we haven't done 2 blocks yet, go back to IDLE
                        if (block_num < 2'd1) begin
                            state <= IDLE;
                        end else begin
                            // All done, stay here
                            state <= WAIT_LOAD_LOW;
                        end
                    end
                end
                
                default: state <= IDLE;
            endcase
        end
    end

endmodule