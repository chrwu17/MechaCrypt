// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/7/2025

////////////////////////////////////////////////////////////
// spramRead.sv()
//      Reads back the stored ciphertext from the iCE40 SPRAM 16 bits at a time.
//      The ciphertext is reconstructed and sent to the sender module for transmission over the actuators.
////////////////////////////////////////////////////////////

module spramRead (
    input  logic        clk,
    input  logic        write_done,
    input  logic [13:0] baseAddr,   
    output logic [127:0] cipher_out,
    output logic        read_done);

    // Internal signals
    logic [15:0] data_word;
    logic [13:0] addr;
    logic [2:0]  idx;

    // FSM to control SPRAM reads
    typedef enum logic [1:0] {IDLE, READ, DONE} statetype;
    statetype state;

    always_ff @(posedge clk) begin
            case (state)
                IDLE: if (write_done) begin
                    addr       <= baseAddr;
                    idx        <= 0;
                    state      <= READ;
                end

                READ: begin
                    cipher_out[idx*16 +: 16] <= data_word; // reconstruct ciphertext
                    addr       <= addr + 1;
                    idx        <= idx + 1;
                    
                    if (idx == 3'd7) begin
                        state <= DONE;
                    end
                end

                DONE: begin
                    read_done <= 1;
                    state     <= IDLE;
                end
            endcase
    end

    // SPRAM read logic
    SB_SPRAM256KA spram_inst (
        .DATAIN     (16'b0),
        .ADDRESS    (addr),
        .MASKWREN   (4'b0000),
        .WREN       (1'b0),
        .CHIPSELECT (1'b1),
        .CLOCK      (clk),
        .STANDBY    (1'b0),
        .SLEEP      (1'b0),
        .POWEROFF   (1'b1),
        .DATAOUT    (data_word));

endmodule

