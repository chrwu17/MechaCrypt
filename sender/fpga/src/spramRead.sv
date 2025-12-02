// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/7/2025

////////////////////////////////////////////////////////////
// spramRead.sv()
//      Reads back the stored ciphertext from the iCE40 SPRAM 16 bits at a time.
//      The ciphertext is reconstructed and sent to the sender module for transmission over the actuators.
////////////////////////////////////////////////////////////

module spramRead (
    input  logic         clk,
    input  logic         write_done,
    input  logic [13:0]  baseAddr,   
    input  logic         send_done,    // Feedback from msgSend
    output logic [127:0] cipher_out,
    output logic         read_done);

    // Internal signals
    logic [15:0] data_word;
    logic [13:0] addr;
    logic [3:0]  idx;
    
    // Edge detection for write_done
    logic write_done_prev;
    logic write_done_pulse;
    
    always_ff @(posedge clk) begin
        write_done_prev <= write_done;
    end
    
    assign write_done_pulse = write_done && !write_done_prev;

    // FSM to control SPRAM reads
    typedef enum logic [1:0] {IDLE, READ, WAIT_SEND} statetype;
    statetype state;

    always_ff @(posedge clk) begin
        case (state)
            IDLE: begin
                read_done <= 0;
                if (write_done_pulse) begin
                    addr  <= baseAddr;
                    idx   <= 0;
                    state <= READ;
                end
            end

            READ: begin
                if (idx < 8) begin
                    cipher_out[idx*16 +: 16] <= data_word; // reconstruct ciphertext
                    addr <= addr + 1;
                    idx  <= idx + 1;
                end
                
                if (idx == 8) begin
                    read_done <= 1;  // Pulse read_done for one cycle
                    state     <= WAIT_SEND;
                end
            end

            WAIT_SEND: begin
                read_done <= 0;  // Clear pulse
                if (send_done) begin
                    state <= IDLE;  // Wait for next encryption
                end
            end
            
            default: state <= IDLE;
        endcase
    end

    // SPRAM read logic
    SP256K spram_inst (
        .AD       (addr),
        .DI       (16'b0),
        .MASKWE   (4'b0000),
        .WE       (1'b0),
        .CS       (1'b1),
        .CK       (clk),
        .STDBY    (1'b0),
        .SLEEP    (1'b0),
        .PWROFF_N (1'b1),
        .DO       (data_word));

endmodule