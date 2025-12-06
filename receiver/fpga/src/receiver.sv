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

