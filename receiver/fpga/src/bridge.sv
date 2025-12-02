// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

////////////////////////////////////////////////////////////
// bridge.sv()
//      Module that receives the total message length in bytes and decryption key from sender MCU (MCU 1).
//      The key is used for the decryption of the ciphertext to retrieve the original message.
//      The length is used to verify the received message as well calculate transfer progress
//      which is then displayed on an LCD screen in form of a progreess bar.
// 
//      They are received through SPI and allows the receiver to have access to such information
//      faster and before the sending through the mechanical actuators finishes.
//      
//      The inputs are shifted in serially and the outputs are shifted out serially as well on the rising edge of sck.
//      Implements two independent masters: MCU1 Receive and MCU2 Send.
////////////////////////////////////////////////////////////

module bridge (
    input  logic sck_in,   // Sender MCU SPI clock
    input  logic sdi,      // Sender MCU MOSI
    input  logic cs_in,    // Sender MCU CS (active low)

    output logic [127:0] decryption_key,  // Current key for AES decryption
    output logic [7:0]   msg_length,      // Total message length (for receiver MCU display)
    output logic         key_ready        // Pulses high when new key is loaded
);

    // Shift register to receive data
    logic [135:0] shift_reg;  // 8 bits msg_length + 128 bits key = 136 bits
    logic [7:0]   bit_count;
    logic         receiving;
    
    initial begin
        decryption_key = 128'h0;
        msg_length = 8'h0;
        key_ready = 1'b0;
        bit_count = 8'd0;
        receiving = 1'b0;
    end
    
    // Receive data from sender MCU
    always_ff @(posedge sck_in) begin
        if (!cs_in) begin  // CS active low
            // Shift in data MSB first
            shift_reg <= {shift_reg[134:0], sdi};
            bit_count <= bit_count + 1;
            receiving <= 1'b1;
            
            // After 136 bits (8 + 128), latch the data
            if (bit_count == 8'd135) begin
                msg_length <= shift_reg[135:128];  // Top 8 bits
                decryption_key <= shift_reg[127:0]; // Bottom 128 bits
                key_ready <= 1'b1;  // Pulse ready signal
                bit_count <= 8'd0;
            end else begin
                key_ready <= 1'b0;
            end
        end else begin
            // CS deasserted - reset for next transmission
            key_ready <= 1'b0;
            bit_count <= 8'd0;
            receiving <= 1'b0;
        end
    end

endmodule



