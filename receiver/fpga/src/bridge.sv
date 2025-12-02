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
    input  logic sck_in,   // MCU1 SPI clk
    input  logic sdi,      // MCU1 MOSI
    input  logic cs_in,    // MCU1 CS

    input  logic sck_out,  // MCU2 SPI clk
    input  logic cs_out,   // MCU2 CS
    output logic sdo,      // MCU2 MISO

    output logic ready     // Trigger to start sending to MCU2
);

    // Internal signals
    logic [127:0] decryptionKey;
    logic [7:0]   msgLength;
    logic [135:0] shiftOut;      // 128+8 = 136 bits to be sent to MCU 2
    logic [7:0]   bitCount;      // counts received bits
    
    logic         sending;       // 0 = receiving, 1 = sending
    logic         sdo_next;

    // Initial signals
    initial begin
        sending  = 1'b0;
        bitCount = 8'd0;
    end
    
    // Receivinng logic: On positive edge of MCU1 sck
    always_ff @(posedge sck_in) begin
        if (!sending && !cs_in) begin
            // Shift data in MSB-first
            {decryptionKey, msgLength} <= {decryptionKey, msgLength, sdi};

            // Prepare sending after 136 bits received
            if (bitCount == 8'd135) begin
                sending  <= 1'b1;
                shiftOut <= {decryptionKey, msgLength};
                bitCount <= 8'd0;
            end
            else begin
                bitCount <= bitCount + 1;
            end
        end

        // Reset counter after CS goes high
        if (cs_in) begin
            bitCount <= 8'd0;
        end
    end 

    // Shiftout logic: On positive edge of MCU2 sck
    always_ff @(posedge sck_out) begin
        if (sending && !cs_out) begin 
            shiftOut <= {shiftOut[134:0], 1'b0};
        end
    end

    // Output logic: On negative edge of MCU2 sck
    always_ff @(negedge sck_out) begin
        if (sending && !cs_out)
            sdo_next <= shiftOut[135]; // current MSB
        else
            sdo_next <= 1'b0;          // idle state
    end

    assign sdo = sdo_next;
    assign ready = sending;

    // Reset signals after sending is done
    always_ff @(posedge cs_out) begin
        sending  <= 1'b0;
        ready    <= 1'b0;
        bitCount <= 8'd0;
        shiftOut <= 136'd0;
    end

endmodule



