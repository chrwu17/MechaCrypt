// Josaphat Ngoga
// jngoga@g.hmc.edu
// 12/4/2025

//////////////////////////////////////////////////////////// 
// msgSend() 
//      Takes in two 128-bit inputs (ciphertext and key, 32 bytes total) and sends them out 
//      one byte at a time over a parallel 8-bit output, synchronized to a slower transfer 
//      clk (tx_clk) that is also sent alongside the data. 
//      It is meant to drive the mechanical actuators of the MechaCrypt system.
////////////////////////////////////////////////////////////
module msgSend #(
    parameter CLK_FREQ = 24_000_000,  // system clock frequency (Set to 48MHz based on HSOSC)
    parameter TX_FREQ  = 3            // Manually choose send frequency (Hz)
    )(
    input  logic         clk,
    input  logic         reset,
    input  logic         start,       // signal to start sending message
    input  logic [127:0] ciphertext,  // 128-bit ciphertext to send
    input  logic [127:0] key,         // 128-bit key to send
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
    logic [5:0] idx; // Byte index (0 to 31 for 32 bytes total)
    logic sending;  
    logic [7:0] msg_out_raw;      // Raw byte from combined message
    
    // Combine ciphertext and key into one 256-bit message
    logic [255:0] msg_combined;
    assign msg_combined = {ciphertext, key}; // Ciphertext in upper 128 bits, key in lower 128 bits
    
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
            if (idx == 32) begin
                idx        <= 0;
                sending    <= 0;
                send_done  <= 1;
            end else begin
                idx <= idx + 1;
            end
        end
    end
    
    // Iteratively select the appropriate byte from msg_combined
    always_comb begin
        case (idx)
            // Ciphertext bytes (0-15)
            5'd0:  msg_out_raw = msg_combined[255:248];
            5'd1:  msg_out_raw = msg_combined[247:240];
            5'd2:  msg_out_raw = msg_combined[239:232];
            5'd3:  msg_out_raw = msg_combined[231:224];
            5'd4:  msg_out_raw = msg_combined[223:216];
            5'd5:  msg_out_raw = msg_combined[215:208];
            5'd6:  msg_out_raw = msg_combined[207:200];
            5'd7:  msg_out_raw = msg_combined[199:192];
            5'd8:  msg_out_raw = msg_combined[191:184];
            5'd9:  msg_out_raw = msg_combined[183:176];
            5'd10: msg_out_raw = msg_combined[175:168];
            5'd11: msg_out_raw = msg_combined[167:160];
            5'd12: msg_out_raw = msg_combined[159:152];
            5'd13: msg_out_raw = msg_combined[151:144];
            5'd14: msg_out_raw = msg_combined[143:136];
            5'd15: msg_out_raw = msg_combined[135:128];
            
            // Key bytes (16-31)
            5'd16: msg_out_raw = msg_combined[127:120];
            5'd17: msg_out_raw = msg_combined[119:112];
            5'd18: msg_out_raw = msg_combined[111:104];
            5'd19: msg_out_raw = msg_combined[103:96];
            5'd20: msg_out_raw = msg_combined[95:88];
            5'd21: msg_out_raw = msg_combined[87:80];
            5'd22: msg_out_raw = msg_combined[79:72];
            5'd23: msg_out_raw = msg_combined[71:64];
            5'd24: msg_out_raw = msg_combined[63:56];
            5'd25: msg_out_raw = msg_combined[55:48];
            5'd26: msg_out_raw = msg_combined[47:40];
            5'd27: msg_out_raw = msg_combined[39:32];
            5'd28: msg_out_raw = msg_combined[31:24];
            5'd29: msg_out_raw = msg_combined[23:16];
            5'd30: msg_out_raw = msg_combined[15:8];
            5'd31: msg_out_raw = msg_combined[7:0];
            
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
            msg_out <= 8'h00;        // Clear output on falling edge
        end
    end
    
endmodule