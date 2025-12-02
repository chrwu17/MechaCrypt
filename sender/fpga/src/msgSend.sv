// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/5/2025

//////////////////////////////////////////////////////////// 
// msgSend() 
//      Takes in a 128-bit input (16 bytes) and sends it outone byte at a time over a parallel 8-bit output,
//      synchronized to a slower transfer clk (tx_clk) that is also sent alongside the data. 
//      It is meant to drive the mechanical actuators of the MechaCrypt system.
////////////////////////////////////////////////////////////

module msgSend #(
    parameter CLK_FREQ = 24_000_000,  // System clock frequency
    parameter TX_FREQ  = 3             // Output transfer clock frequency (Hz)
)(
    input  logic         clk,
    input  logic         reset,
    input  logic         start,       // Begin sending message
    input  logic [127:0] msg_in,      // 128-bit input message
    output logic [7:0]   msg_out,     // Current output byte
    output logic         tx_clk,      // Transfer clock output
    output logic         send_done    // High after the 16-byte sequence is complete
);

    //===========================================
    // CLOCK DIVIDER (Generates tx_clk_internal)
    //===========================================

    localparam integer DIV_COUNT = CLK_FREQ / (2 * TX_FREQ);

    logic [31:0] counter;
    logic tx_clk_internal, tx_clk_prev;
    logic tx_clk_posedge;

    logic sending;
    logic [4:0] idx;      // 0–15 valid for byte selection
    logic last_byte_sent; // Flag to track when last byte has been clocked out

    // Active whenever data is being clocked out
    logic active;
    assign active = sending;

    // Clock generator runs ONLY when active
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            counter         <= 0;
            tx_clk_internal <= 0;
            tx_clk_prev     <= 0;
        end else begin
            tx_clk_prev <= tx_clk_internal;

            if (active) begin
                if (counter == DIV_COUNT - 1) begin
                    counter         <= 0;
                    tx_clk_internal <= ~tx_clk_internal;
                end else begin
                    counter <= counter + 1;
                end
            end else begin
                counter         <= 0;
                tx_clk_internal <= 0;   // freeze clock low when not active
            end
        end
    end

    // Rising edge of tx_clk_internal
    assign tx_clk_posedge = tx_clk_internal && !tx_clk_prev;

    // Output tx_clk only when sending AND during high phase
    assign tx_clk = (active && tx_clk_internal) ? 1'b1 : 1'b0;


    //=========================
    // SEND STATE MACHINE
    //=========================

    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            sending        <= 0;
            idx            <= 0;
            send_done      <= 0;
            last_byte_sent <= 0;

        end else if (start && !sending) begin
            // Start new send (only if not already done)
            sending        <= 1;
            idx            <= 0;
            send_done      <= 0;
            last_byte_sent <= 0;

        end else if (sending) begin
            if (tx_clk_posedge) begin
                if (last_byte_sent) begin
                    // Last byte has been clocked out, stop now
                    sending        <= 0;
                    idx            <= 0;
                    send_done      <= 1;
                    last_byte_sent <= 0;
                end else if (idx == 15) begin
                    // This is the 16th byte - mark it but keep idx at 15
                    last_byte_sent <= 1;
                end else begin
                    // Normal byte increment
                    idx <= idx + 1;
                end
            end

        end
    end


    //=========================
    // BYTE SELECTION LOGIC
    //=========================

    logic [7:0] msg_byte;

    always_comb begin
        case (idx)
            5'd0:  msg_byte = msg_in[127:120];
            5'd1:  msg_byte = msg_in[119:112];
            5'd2:  msg_byte = msg_in[111:104];
            5'd3:  msg_byte = msg_in[103:96];
            5'd4:  msg_byte = msg_in[95:88];
            5'd5:  msg_byte = msg_in[87:80];
            5'd6:  msg_byte = msg_in[79:72];
            5'd7:  msg_byte = msg_in[71:64];
            5'd8:  msg_byte = msg_in[63:56];
            5'd9:  msg_byte = msg_in[55:48];
            5'd10: msg_byte = msg_in[47:40];
            5'd11: msg_byte = msg_in[39:32];
            5'd12: msg_byte = msg_in[31:24];
            5'd13: msg_byte = msg_in[23:16];
            5'd14: msg_byte = msg_in[15:8];
            5'd15: msg_byte = msg_in[7:0];
            default: msg_byte = 8'h00;
        endcase
    end

    // Only output a byte when tx_clk is high AND sending
    assign msg_out = (sending && tx_clk_internal) ? msg_byte : 8'h00;

endmodule