// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

////////////////////////////////////////////////////////////
// spramWrite.sv()
//      Stores the ciphertext from the encryption module using iCE40 SPRAM primitive SB_SPRAM256KA 
//      configured with a 16K x 16-bit memory layout. 
//      Each 128-bit ciphertext is split into eight 16-bit words and stored sequentially. 
//      SPRAM write operations are triggered by the 'done' signal from the encryption module.
//
//      Section 2.1 of iCE40 SPRAM Usage Guide
////////////////////////////////////////////////////////////

module spramWrite (
    input  logic         clk,          // System clock
    input  logic         done,         // Trigger to start writing (AES encryption done)
    input  logic [127:0] ciphertext,   // 128-bit AES ciphertext input
    output logic [13:0] baseAddr,      // Starting address for SPRAM write
    output logic  write_done);         // Done writing signal

    // Ensure done signal is synchronized to this clk domain
    logic done_meta, done_delayed, done_sync;
    logic load;

    always_ff @(posedge clk) begin
        done_meta    <= done;
        done_sync    <= done_meta;
        done_delayed <= done_sync; // For edge detection
    end

    assign load = done_sync & ~done_delayed; // Single-cycle pulse

    // Split ciphertext into 8 words (16 bits each)
    logic [15:0] data_in [7:0];
    logic [13:0] addr;
    logic [2:0] idx;
    logic        we_reg;

    assign {
        data_in[7], data_in[6], data_in[5], data_in[4],
        data_in[3], data_in[2], data_in[1], data_in[0]
    } = ciphertext;

    
    // FSM to control SPRAM writes
    typedef enum logic [1:0] {IDLE, WRITE, DONE} statetype;
    statetype state;

    always_ff @(posedge clk) begin
        case (state)
            IDLE: begin
                we_reg <= 0;
                write_done <= 0;
                if (load) begin
                    addr        <= 14'd0;
                    baseAddr    <= 14'd0;
                    idx         <= 3'd0;
                    we_reg      <= 1;
                    state       <= WRITE;
                end
            end

            WRITE: begin
                we_reg     <= 1;
                addr       <= idx;       // Store word
                idx        <= idx + 1;

                if (idx == 3'd7) begin
                    state  <= DONE;
                end
            end

            DONE: begin
                we_reg <= 0;
                write_done <= 1; // write complete
                state  <= IDLE; // reset for next load
            end

            default: state <= IDLE;
        endcase
    end

    // SPRAM instantiation and write logic
    genvar j;
    generate
        for (j = 0; j < 8; j++) begin : spram_block
            SB_SPRAM256KA spram_inst (
                .ADDRESS    (addr),
                .DATAIN     (data_in[j]),
                .MASKWREN   (4'b1111),
                .WREN       (we_reg),
                .CHIPSELECT (1'b1),
                .CLOCK      (clk),
                .STANDBY    (1'b0),
                .SLEEP      (1'b0),
                .POWEROFF   (1'b1),
                .DATAOUT    ()
            );
        end
    endgenerate
endmodule
