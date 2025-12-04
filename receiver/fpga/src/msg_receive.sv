// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

//////////////////////////////////////////////////////////// 
// msg_receive() 
//   Receives 8-bit data bytes synchronized to a slower transfer clock (tx_clk) from the sender module,
//   and reconstructs them into a 128-bit output message (16 bytes).
//   It is meant to receive data from the activated limit switches of the MechaCrypt system.
//   
//   Added SPI output capability to shift the reconstructed message to an MCU.
////////////////////////////////////////////////////////////

module msg_receive #(
    parameter TOTAL_BYTES = 16    // total bytes expected (for 128-bit word)
    )(
    input  logic         clk,     // FPGA system clock
    input  logic         reset,
    input  logic         tx_clk,
    input  logic [7:0]   data_in, // 8 parallel data lines from sender
    
    // SPI interface to MCU
    input  logic         sck,     // MCU SPI clock
    input  logic         cs,      // MCU chip select (active low)
    output logic         sdo,     // MCU MISO
    
    output logic [127:0] msg_out, // reconstructed 128-bit array
    output logic         done,    // high when full array received
    output logic         ready);  // high when ready to send to MCU

    // Sync tx_clk to FPGA clk domain
    logic tx_clk_sync_0, tx_clk_sync_1;
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            tx_clk_sync_0 <= 0;
            tx_clk_sync_1 <= 0;
        end else begin
            tx_clk_sync_0 <= tx_clk;
            tx_clk_sync_1 <= tx_clk_sync_0;
        end
    end

    // Detect rising edge of synchronized tx_clk
    logic tx_clk_rise;
    assign tx_clk_rise = (tx_clk_sync_0 && !tx_clk_sync_1);

    // Assemble received bytes into 128-bit msg_out
    logic [3:0] idx;
    logic [127:0] buffer;
    logic receiving;

    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            idx        <= 0;
            buffer     <= 0;
            receiving  <= 0;
            done       <= 0;
        end 
        else begin
            if (tx_clk_rise) begin
                receiving <= 1'b1;
                done      <= 1'b0;

                // shift received byte into buffer
                buffer <= {buffer[119:0], data_in};

                if (idx == (TOTAL_BYTES - 1)) begin
                    done       <= 1'b1;
                    receiving  <= 1'b0;
                    idx        <= 0;
                end else begin
                    idx        <= idx + 1;
                end
            end
        end
    end

    // Output the reconstructed message
    assign msg_out = buffer;

    // ========== SPI Output Logic ==========
    logic [127:0] shiftOut;
    logic         sending;
    logic         sdo_next;
    logic         done_prev;
    
    // Synchronize sck to clk domain
    logic sck_sync_0, sck_sync_1, sck_prev;
    logic sck_rise, sck_fall;
    
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
    
    assign sck_rise = sck_sync_1 && !sck_prev;
    assign sck_fall = !sck_sync_1 && sck_prev;

    // Synchronize cs to clk domain
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

    // Combined SPI logic - all driven from clk domain
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            done_prev <= 1'b0;
            sending   <= 1'b0;
            shiftOut  <= 128'd0;
            sdo_next  <= 1'b0;
        end 
        else begin
            // Track done signal
            done_prev <= done;
            
            // Detect done rising edge and load data
            if (done && !done_prev) begin
                sending  <= 1'b1;
                shiftOut <= buffer;
                sdo_next <= buffer[127]; // Load first bit immediately
            end
            
            // Reset only when CS goes high AND we were actively sending
            if (cs_rise && sending) begin
                sending  <= 1'b0;
                sdo_next <= 1'b0;
            end
            
            // Shift data on rising edge of sck
            if (sck_rise && sending && !cs_sync_1) begin
                shiftOut <= {shiftOut[126:0], 1'b0};
                sdo_next <= shiftOut[126]; // Output next bit (before shift completes)
            end
        end
    end

    assign sdo = sdo_next;
    assign ready = sending;

endmodule