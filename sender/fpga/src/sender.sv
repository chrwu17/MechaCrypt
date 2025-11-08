// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/7/2025

////////////////////////////////////////////////////////////
// sender.sv()
//      Top-level sender module that integrates AES encryption, SPRAM storage, and message sending.
////////////////////////////////////////////////////////////

module sender (
    input logic reset,          // FPGA reset
    input logic sck,            // SPI clock
    input logic sdi,            // SPI data in
    input logic load,           // Load signal to start encryption
    output logic sdo,           // SPI data out
    output logic [7:0] msg_out, // 8-bit message output
    output logic tx_clk,         // Transfer clock output
    output logic send_done     // Sending done signal
);

    // Internal high-speed oscillator
    logic clk;
    HSOSC hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk)); // 48 MHz    

    // AES Encryption Module
    logic done;
    logic [127:0] ciphertext;

    aesEncryption encrypt (
        .clk        (clk),
        .sck        (sck),
        .sdi        (sdi),
        .load       (load),
        .sdo        (sdo),
        .cyphertext (ciphertext),
        .done       (done)
    );

    // SPRAM Write Module
    logic write_done;
    logic [13:0] baseAddr;
    
    spramWrite write (
        .clk         (clk),
        .done        (done),
        .ciphertext  (ciphertext),
        .baseAddr    (baseAddr),
        .write_done  (write_done)
    );

    // SPRAM Read Module
    logic read_done;
    logic [127:0] cipher_out;

    spramRead read (
        .clk        (clk),
        .write_done (write_done),
        .baseAddr   (baseAddr),
        .cipher_out (cipher_out),
        .read_done  (read_done)
    );

    // Message Sending Module
    msgSend send (
        .clk       (clk),
        .reset     (reset),
        .start     (read_done),
        .msg_in    (cipher_out),
        .msg_out   (msg_out),
        .tx_clk    (tx_clk),
        .send_done (send_done)
    );

endmodule