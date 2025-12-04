// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/5/2025
//////////////////////////////////////////////////////////// 
// msgSend() 
//      Takes in a 128-bit input (16 bytes) and sends it out one byte at a time over a parallel 8-bit output,
//      synchronized to a slower transfer clk (tx_clk) that is also sent alongside the data. 
//      It is meant to drive the mechanical actuators of the MechaCrypt system.
////////////////////////////////////////////////////////////
module msgSend #(
    parameter CLK_FREQ = 24_000_000,  // system clock frequency (Set to 48MHz based on HSOSC)
    parameter TX_FREQ  = 3            // Manually choose send frequency (Hz)
    )(
    input  logic         clk,
    input  logic         reset,
    input  logic         start,       // signal to start sending message
    input  logic [127:0] msg_in,      // 128-bit input message to send
    output logic [7:0]   msg_out,     // 8-bit output message (one byte at a time)
    output logic         tx_clk,      // transfer clock output
    output logic         send_done);  // signal indicating message has been fully sent  
    
    // Generate tx_clk based on CLK_FREQ and TX_FREQ
    localparam integer DIV_COUNT = CLK_FREQ / (2 * TX_FREQ); 
    logic [31:0] counter;
    logic tx_clk_prev;
    logic tx_clk_rising;
    logic tx_clk_falling;
    
    // Byte sending control logic
    logic [4:0] idx; // Byte index in msg_in (0 to 15)
    logic sending;  
    logic [7:0] msg_out_raw;      // Raw byte from msg_in
    
    // Clock generation - only toggle when sending
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            counter   <= 0;
            tx_clk    <= 0;
        end else if (sending) begin  // Only toggle when actively sending
            if (counter == DIV_COUNT - 1) begin
                counter   <= 0;
                tx_clk    <= ~tx_clk;
            end else begin
                counter   <= counter + 1;
            end
        end else begin
            // Not sending - reset clock to 0
            counter   <= 0;
            tx_clk    <= 0;
        end
    end
    
    // Detect rising and falling edges of tx_clk
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            tx_clk_prev <= 0;
        end else begin
            tx_clk_prev <= tx_clk;
        end
    end
    
    assign tx_clk_rising = tx_clk && !tx_clk_prev;
    assign tx_clk_falling = !tx_clk && tx_clk_prev;
    
    // Byte sending control logic
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            idx        <= 0;
            sending    <= 0;
            send_done  <= 0;
        end else if (start && !sending) begin
            sending    <= 1;
            idx        <= 0;
            send_done  <= 0;
        end else if (sending && tx_clk_rising) begin
            // advance on each tx_clk rising edge
            if (idx == 16) begin
                idx        <= 0;
                sending    <= 0;
                send_done  <= 1;
            end else begin
                idx <= idx + 1;
            end
        end
    end
    
    // Iteratively select the appropriate byte from msg_in
    always_comb begin
        case (idx)
            4'd0:  msg_out_raw = msg_in[127:120];
            4'd1:  msg_out_raw = msg_in[119:112];
            4'd2:  msg_out_raw = msg_in[111:104];
            4'd3:  msg_out_raw = msg_in[103:96];
            4'd4:  msg_out_raw = msg_in[95:88];
            4'd5:  msg_out_raw = msg_in[87:80];
            4'd6:  msg_out_raw = msg_in[79:72];
            4'd7:  msg_out_raw = msg_in[71:64];
            4'd8:  msg_out_raw = msg_in[63:56];
            4'd9:  msg_out_raw = msg_in[55:48];
            4'd10: msg_out_raw = msg_in[47:40];
            4'd11: msg_out_raw = msg_in[39:32];
            4'd12: msg_out_raw = msg_in[31:24];
            4'd13: msg_out_raw = msg_in[23:16];
            4'd14: msg_out_raw = msg_in[15:8];
            4'd15: msg_out_raw = msg_in[7:0];
            default: msg_out_raw = 8'h00;
        endcase
    end
    
    // Output the byte only when tx_clk is high, go low when tx_clk is low
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            msg_out <= 8'h00;
        end else if (tx_clk_rising) begin
            msg_out <= msg_out_raw;  // Load new byte on rising edge
        end else if (tx_clk_falling) begin
            msg_out <= 8'h00;        // Clear output on falling edge (creates the "blink")
        end
    end
    
endmodule
