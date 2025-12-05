// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

// Top-level Receiver Module
// Receives 32 bytes (cyphertext + key), decrypts, and transmits plaintext via SPI

module receiver_top(
    input  logic          reset,
    input  logic          tx_clk,
    input  logic [7:0]    data_in,
    
    // SPI interface to MCU
    input  logic          sck,
    input  logic          cs,
    output logic          sdo,
    
    // Debug outputs
    output logic [4:0]    debug_byte_count,
    output logic          debug_tx_activity,
    output logic          debug_debounce_locked,
    output logic          debug_tx_clk_raw,
    output logic          debug_tx_clk_synced,
    output logic          debug_decryption_done,
    output logic          debug_spi_ready,
    output logic          debug_spi_busy,
    
    // Status LEDs
    output logic          led_receiving,
    output logic          led_decrypting,
    output logic          led_transmitting);

    // Internal clock
    logic clk;
    HSOSC #(.CLKHF_DIV(2'b01)) 
          hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk)); // 24 MHz

    // Signals between modules
    logic [127:0] cyphertext, key, plaintext;
    logic msg_done, decrypt_done, spi_load;
    logic spi_ready, spi_busy;
    
    // ========== Message Receiver ==========
    msg_receive msg_rx(
        .reset(reset),
        .tx_clk(tx_clk),
        .data_in(data_in),
        .cyphertext(cyphertext),
        .key(key),
        .done(msg_done),
        .debug_byte_count(debug_byte_count),
        .debug_tx_activity(debug_tx_activity),
        .debug_debounce_locked(debug_debounce_locked),
        .debug_tx_clk_raw(debug_tx_clk_raw),
        .debug_tx_clk_synced(debug_tx_clk_synced)
    );

    // ========== State Machine for Control ==========
    typedef enum logic [1:0] {
        IDLE,
        DECRYPT,
        TRANSMIT
    } state_t;
    
    state_t state;
    logic decrypt_start;
    
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            state         <= IDLE;
            decrypt_start <= 1'b0;
            spi_load      <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    decrypt_start <= 1'b0;
                    spi_load      <= 1'b0;
                    if (msg_done) begin
                        state         <= DECRYPT;
                        decrypt_start <= 1'b1;
                    end
                end
                
                DECRYPT: begin
                    decrypt_start <= 1'b0;
                    if (decrypt_done) begin
                        state    <= TRANSMIT;
                        spi_load <= 1'b1;
                    end
                end
                
                TRANSMIT: begin
                    spi_load <= 1'b0;
                    if (!spi_busy && !cs) begin  // Transmission complete
                        state <= IDLE;
                    end
                end
                
                default: state <= IDLE;
            endcase
        end
    end

    // ========== AES Decryption Core ==========
    aes_core_wrapper aes_decrypt(
        .clk(clk),
        .reset(reset),
        .cyphertext(cyphertext),
        .key(key),
        .start(decrypt_start),
        .plaintext(plaintext),
        .done(decrypt_done)
    );

    // ========== SPI Transmitter ==========
    spi_transmitter spi_tx(
        .clk(clk),
        .reset(reset),
        .plaintext(plaintext),
        .load(spi_load),
        .sck(sck),
        .cs(cs),
        .sdo(sdo),
        .ready(spi_ready),
        .busy(spi_busy)
    );

    // ========== Status LEDs ==========
    assign led_receiving    = (state == IDLE) && !msg_done;
    assign led_decrypting   = (state == DECRYPT);
    assign led_transmitting = (state == TRANSMIT);
    
    // ========== Debug Outputs ==========
    assign debug_decryption_done = decrypt_done;
    assign debug_spi_ready       = spi_ready;
    assign debug_spi_busy        = spi_busy;

endmodule

// Simplified AES Core Wrapper
// Takes cyphertext and key as inputs, outputs plaintext when done

module aes_core_wrapper(
    input  logic          clk,
    input  logic          reset,
    input  logic [127:0]  cyphertext,
    input  logic [127:0]  key,
    input  logic          start,          // Start decryption
    
    output logic [127:0]  plaintext,
    output logic          done);

    logic load_sync;
    
    // Generate load pulse
    logic start_prev;
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            start_prev <= 1'b0;
            load_sync  <= 1'b0;
        end else begin
            start_prev <= start;
            load_sync  <= start && !start_prev;  // Rising edge
        end
    end

    // Instantiate AES core
    aes_core core(
        .clk(clk),
        .load(load_sync),
        .key(key),
        .cyphertext(cyphertext),
        .done_decrypt(done),
        .plaintext(plaintext)
    );

endmodule