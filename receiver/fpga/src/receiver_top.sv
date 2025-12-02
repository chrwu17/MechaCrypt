// Josaphat Ngoga & Christian Wu
// Receiver FPGA Top Module
// Integrates: Bridge (key receiver) + msg_receive (mechanical input) + AES decryption

module receiver_top (
    // Bridge interface (from sender MCU)
    input  logic bridge_sck,
    input  logic bridge_sdi,
    input  logic bridge_cs,
    
    // Mechanical system interface (8 parallel data lines + transfer clock)
    input  logic [7:0] mech_data,
    input  logic mech_tx_clk,
    
    // Internal clock for AES
    input  logic clk,
    input  logic reset,
    
    // Outputs to receiver MCU (for display/storage)
    output logic [127:0] plaintext_out,
    output logic plaintext_ready
);

    // Bridge outputs
    logic [127:0] current_key;
    logic [7:0]   msg_length;
    logic         key_ready;
    
    // Message receiver outputs
    logic [127:0] ciphertext;
    logic         ciphertext_ready;
    
    // AES decryption signals
    logic         aes_load;
    logic         aes_done;
    logic [127:0] aes_plaintext;
    
    // ==================== BRIDGE MODULE ====================
    // Receives keys from sender MCU just-in-time
    bridge bridge_inst (
        .sck_in(bridge_sck),
        .sdi(bridge_sdi),
        .cs_in(bridge_cs),
        .decryption_key(current_key),
        .msg_length(msg_length),
        .key_ready(key_ready)
    );
    
    // ==================== MECHANICAL MESSAGE RECEIVER ====================
    // Receives ciphertext blocks from mechanical system
    msg_receive #(
        .TOTAL_BYTES(16)
    ) msg_rx (
        .clk(clk),
        .reset(reset),
        .tx_clk(mech_tx_clk),
        .data_in(mech_data),
        .msg_out(ciphertext),
        .done(ciphertext_ready)
    );
    
    // ==================== AES DECRYPTION CORE ====================
    aes_core aes_decrypt (
        .clk(clk),
        .load(aes_load),
        .key(current_key),
        .cyphertext(ciphertext),
        .done_decrypt(aes_done),
        .plaintext(aes_plaintext)
    );
    
    // ==================== CONTROL LOGIC ====================
    // Start decryption when ciphertext arrives from mechanical system
    typedef enum logic [1:0] {
        IDLE,
        WAIT_KEY,
        DECRYPT,
        OUTPUT
    } state_t;
    
    state_t state;
    logic ciphertext_latched;
    
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            state <= IDLE;
            aes_load <= 1'b0;
            plaintext_ready <= 1'b0;
            ciphertext_latched <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    plaintext_ready <= 1'b0;
                    aes_load <= 1'b0;
                    
                    // Wait for ciphertext from mechanical system
                    if (ciphertext_ready && !ciphertext_latched) begin
                        ciphertext_latched <= 1'b1;
                        
                        // Check if we already have a key
                        if (key_ready) begin
                            state <= DECRYPT;
                            aes_load <= 1'b1;
                        end else begin
                            state <= WAIT_KEY;
                        end
                    end
                end
                
                WAIT_KEY: begin
                    // Ciphertext is ready but key hasn't arrived yet
                    if (key_ready) begin
                        state <= DECRYPT;
                        aes_load <= 1'b1;
                    end
                end
                
                DECRYPT: begin
                    aes_load <= 1'b0;  // Pulse load signal
                    
                    // Wait for decryption to complete
                    if (aes_done) begin
                        plaintext_out <= aes_plaintext;
                        plaintext_ready <= 1'b1;
                        state <= OUTPUT;
                    end
                end
                
                OUTPUT: begin
                    // Hold output for receiver MCU to read
                    // Reset when ciphertext_ready goes low (ready for next block)
                    if (!ciphertext_ready) begin
                        ciphertext_latched <= 1'b0;
                        plaintext_ready <= 1'b0;
                        state <= IDLE;
                    end
                end
                
                default: state <= IDLE;
            endcase
        end
    end

endmodule