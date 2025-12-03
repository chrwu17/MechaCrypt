// Josaphat Ngoga
// jngoga@g.hmc.edu
// 12/1/2025

////////////////////////////////////////////////////////////
// receiver.sv()
//      Top-level receiver module that integrates AES encryption, SPRAM storage, and message sending.
////////////////////////////////////////////////////////////

module receiver (
    // Bridge SPI (MCU1 -> FPGA) for receiving key and message length
    input  logic sck_1,          // MCU1 SPI clock
    input  logic sdi_1,          // MCU1 MOSI (key + length input)
    input  logic cs_1,           // MCU1 CS
    
    // Decryption SPI (MCU2 <-> FPGA) for ciphertext in / plaintext out
    input  logic sck_2,          // MCU2 SPI clock
    input  logic sdi_2,          // MCU2 MOSI (ciphertext input)
    input  logic cs_2,           // MCU2 CS
    input  logic load,           // MCU2 load signal to start decryption
    output logic sdo_2,          // MCU2 MISO (key, message length, and plaintext output)
    
    // Status outputs
    output logic bridge_ready    // Bridge has received key/length, ready for decryption
    output logic done_decrypt,   // Decryption complete signal
);

    // Internal high-speed oscillator
    logic clk;
    HSOSC #(.CLKHF_DIV(2'b01)) 
          hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk)); // 24 MHz

    // Bridge signals (receives key and message length from MCU1)    
    bridge keyBridge (
        .sck_in  (sck_1),
        .sdi     (sdi_1),
        .cs_in   (cs_1),
        .sck_out (sck_2),          
        .cs_out  (cs_2),           
        .sdo     (sdo_2),          
        .ready   (bridge_ready)
    );
    
    // AES Decryption Module (receives ciphertext from MCU2 via SPI, outputs plaintext)
    aesDecryption decrypt (
        .clk          (clk),
        .sck          (sck_2),
        .sdi          (sdi_2),
        .cs           (cs_2),  
        .load         (load),   
        .sdo          (sdo_2),  
        .done_decrypt (done_decrypt) 
    );

endmodule