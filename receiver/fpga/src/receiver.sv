// Josaphat Ngoga
// jngoga@g.hmc.edu
// 12/1/2025

////////////////////////////////////////////////////////////
// receiver.sv()
//      Top-level receiver module that integrates message reception from mechanical actuators,
//      AES decryption, and SPI communication with MCU.
////////////////////////////////////////////////////////////

module receiver (
    // Message reception from mechanical actuators
    input  logic         tx_clk,         // Transfer clock from sender (slow)
    input  logic [7:0]   data_in,        // 8 parallel data lines from limit switches
    
    // SPI interface with MCU for key/ciphertext input and plaintext output
    input  logic sck,                    // MCU SPI clock
    input  logic sdi,                    // MCU MOSI (key + ciphertext input)
    input  logic cs_msg,                 // MCU CS for message readout
    input  logic cs_decrypt,             // MCU CS for decryption
    input  logic load_decrypt,           // MCU load signal to start decryption
    output logic sdo_msg,                // MCU MISO for message readout
    output logic sdo_decrypt,            // MCU MISO for decryption
    
    // Status outputs
    output logic msg_ready,              // Message received and ready to send via SPI
    output logic done_decrypt,           // Decryption complete signal
    output logic led1,
    output logic led2
);

    // Internal high-speed oscillator
    logic clk;
    logic reset;
    HSOSC #(.CLKHF_DIV(2'b11)) 
          hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk)); // 24 MHz

    // Generate reset signal
    initial begin
        reset = 1'b0;
        #100;
        reset = 1'b1;
    end

    // Message reception signals
    logic [127:0] received_msg;
    logic msg_done;

    // Message Reception Module (receives 128-bit encrypted message from mechanical actuators)
    msg_receive #(
        .TOTAL_BYTES(16)        // 128 bits = 16 bytes
    ) msg_receiver (
        .clk      (clk),
        .reset    (reset),
        .tx_clk   (tx_clk),
        .data_in  (data_in),
        
        // SPI interface for sending received message to MCU
        .sck      (sck),
        .cs       (cs_msg),
        .sdo      (sdo_msg),
        
        .msg_out  (received_msg),
        .done     (msg_done),
        .ready    (msg_ready)
    );

    // AES Decryption Module (receives key + ciphertext from MCU via SPI, outputs plaintext)
    aesDecryption decrypt (
        .clk          (clk),
        .sck          (sck),
        .sdi          (sdi),
        .cs           (cs_decrypt),  
        .load         (load_decrypt),   
        .sdo          (sdo_decrypt),  
        .done_decrypt (done_decrypt),
        .led1         (led1),
        .led2         (led2)
    );

endmodule