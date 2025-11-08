// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

//////////////////////////////////////////////////////////// 
// msg_receive() 
//   Receives 8-bit data bytes synchronized to a slower transfer clock (tx_clk) from the sender module,
//   and reconstructs them into a 128-bit output message (16 bytes).
//   It is meant to receive data from the activated limit switches of the MechaCrypt system.
////////////////////////////////////////////////////////////

module msg_receive #(
    parameter TOTAL_BYTES = 16    // total bytes expected (for 128-bit word)
    )(
    input  logic         clk,     // FPGA system clock
    input  logic         reset,
    input  logic         tx_clk,
    input  logic [7:0]   data_in, // 8 parallel data lines from sender
    output logic [127:0] msg_out, // reconstructed 128-bit array
    output logic         done);   // high when full array received

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
endmodule
