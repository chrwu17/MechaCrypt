`timescale 1ns/1ns
// Josaphat Ngoga
// jngoga@g.hmc.edu
// Modified: 12/4/2025

/////////////////////////////////////////////
// msg_receive_tb()
// Testbench for ability to receive bytes one at a time, reconstruct message array,
// and shift out via SPI to MCU
// Updated for 256-bit output (ciphertext + key)
/////////////////////////////////////////////

module msg_receive_tb();

    // Parameters
    localparam integer TOTAL_BYTES   = 32;  // 16 bytes ciphertext + 16 bytes key

    // Signals
    logic clk, reset, tx_clk;
    logic [7:0] data_in;
    logic [127:0] ciphertext, key;
    logic done, ready;
    
    // SPI signals
    logic sck, cs, sdo;

    // Reference message
    logic [127:0] expected_ciphertext = 128'hDEADBEEF_F00DBABE_12345678_ABCDEF00;
    logic [127:0] expected_key        = 128'hFEDCBA98_76543210_CAFEBABE_DEADC0DE;
    logic [255:0] expected_data = {expected_ciphertext, expected_key};
    logic [255:0] received_spi_data;
    logic pass = 1;

    // Instantiate DUT
    msg_receive #(
        .TOTAL_BYTES(TOTAL_BYTES)
    ) dut (
        .clk(clk),
        .reset(reset),
        .tx_clk(tx_clk),
        .data_in(data_in),
        .sck(sck),
        .cs(cs),
        .sdo(sdo),
        .ciphertext(ciphertext),
        .key(key),
        .done(done),
        .ready(ready)
    );

    // Generate clock
    always begin
        clk = 1'b0; #10;
        clk = 1'b1; #10;
    end

    // Monitor signals
    initial begin
        $monitor("[%0t ns] tx_clk=%b data_in=%h done=%b ready=%b cs=%b sck=%b sdo=%b idx=%0d", 
                  $time, tx_clk, data_in, done, ready, cs, sck, sdo, dut.idx);
    end

    // Stimulus
    integer i;
    logic [7:0] test_bytes [0:31];

    initial begin
        // Ciphertext (DEADBEEF_F00DBABE_12345678_ABCDEF00)
        test_bytes[0]  = 8'hDE;
        test_bytes[1]  = 8'hAD;
        test_bytes[2]  = 8'hBE;
        test_bytes[3]  = 8'hEF;
        test_bytes[4]  = 8'hF0;
        test_bytes[5]  = 8'h0D;
        test_bytes[6]  = 8'hBA;
        test_bytes[7]  = 8'hBE;
        test_bytes[8]  = 8'h12;
        test_bytes[9]  = 8'h34;
        test_bytes[10] = 8'h56;
        test_bytes[11] = 8'h78;
        test_bytes[12] = 8'hAB;
        test_bytes[13] = 8'hCD;
        test_bytes[14] = 8'hEF;
        test_bytes[15] = 8'h00;
        
        // Key (FEDCBA98_76543210_CAFEBABE_DEADC0DE)
        test_bytes[16] = 8'hFE;
        test_bytes[17] = 8'hDC;
        test_bytes[18] = 8'hBA;
        test_bytes[19] = 8'h98;
        test_bytes[20] = 8'h76;
        test_bytes[21] = 8'h54;
        test_bytes[22] = 8'h32;
        test_bytes[23] = 8'h10;
        test_bytes[24] = 8'hCA;
        test_bytes[25] = 8'hFE;
        test_bytes[26] = 8'hBA;
        test_bytes[27] = 8'hBE;
        test_bytes[28] = 8'hDE;
        test_bytes[29] = 8'hAD;
        test_bytes[30] = 8'hC0;
        test_bytes[31] = 8'hDE;
        
        // Initialize signals
        clk     = 0;
        tx_clk  = 0;
        reset   = 0;
        data_in = 8'h00;
        sck     = 0;
        cs      = 1;  // CS high = inactive
        
        #100;
        reset = 1;
        #100;

        $display("=== Starting Message Reception Test ===");
        
        // Send bytes and toggle tx_clk
        for (i = 0; i < TOTAL_BYTES; i = i + 1) begin
            data_in = test_bytes[i];
            #500;
            tx_clk = 1;
            #500;
            tx_clk = 0;
        end

        // Wait for done signal
        $display("Waiting for message reception to complete...");
        wait(done);
        $display("[%0t ns] Message reception complete.", $time);
        $display("Expected ciphertext = %032h", expected_ciphertext);
        $display("Received ciphertext = %032h", ciphertext);
        $display("\nExpected key        = %032h", expected_key);
        $display("Received key        = %032h", key);

        if (ciphertext === expected_ciphertext && key === expected_key) begin
            $display("Parallel data matches expected message.");
        end else begin
            $display("Parallel data mismatch!");
            if (ciphertext !== expected_ciphertext) 
                $display("  Ciphertext mismatch!");
            if (key !== expected_key) 
                $display("  Key mismatch!");
            pass = 0;
        end

        // Wait for ready signal
        #100;
        $display("\nWaiting for ready signal...");
        wait(ready);
        $display("[%0t ns] Module ready to send via SPI.", $time);

        $display("\n");
        $display("===   Starting SPI Readback Test    ===");
 

        // Small delay before starting SPI
        #200;
        
        // Start SPI transaction (activate CS)
        $display("Activating CS (chip select)");
        cs = 0;
        #100;
        
        // Clock out 256 bits via SPI
        $display("Clocking out 256 bits via SPI...");
        $display("First 16 bits:");
        received_spi_data = 256'd0;
        for (i = 0; i < 256; i = i + 1) begin
            // Sample data before rising edge
            #50;
            received_spi_data[255 - i] = sdo;
            if (i < 16) $display("  Bit %3d: sdo=%b", i, sdo);
            
            // Rising edge of sck (triggers shift)
            sck = 1;
            #100;
            
            // Falling edge of sck
            sck = 0;
            #50;
        end

        // End SPI transaction (deactivate CS)
        #100;
        $display("Deactivating CS");
        cs = 1;
        #200;

        // Verify SPI data
        $display("\n");
        $display("===        SPI Test Results         ===");
 
        $display("Expected SPI = %064h", expected_data);
        $display("Received SPI = %064h", received_spi_data);

        if (received_spi_data === 256'bx) begin
            $display("ERROR: SPI data is undefined");
            pass = 0;
        end else if (received_spi_data === expected_data) begin
            $display("SPI data matches expected message.");
        end else begin
            $display("SPI data mismatch!");
            pass = 0;
        end

        // Final results
        #1000;
        $display("\n");
        if (pass) begin
            $display("===      ALL TESTS PASSED!          ===");
        end else begin
            $display("=== ERROR: One or more tests failed ===");
        end
        $display"\n");
        
        $stop;
    end

endmodule