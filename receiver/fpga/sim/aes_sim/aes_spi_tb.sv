// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/26/2025

`timescale 10ns/1ns

/////////////////////////////////////////////
// testbench_aes_spi ()
// Tests AES with cases from FIPS-197 appendix
// Simulates full system with SPI load
/////////////////////////////////////////////

module testbench_aes_spi();
    logic clk, load, done_decrypt, sck, sdi, sdo;
    logic [127:0] key, plaintext, cyphertext, expected;
    logic [255:0] data_in;
    logic [127:0] data_out;
    logic pass = 1;

    // device under test
    aesDecryption dut(clk, sck, sdi, load, sdo, done_decrypt);

    // Monitor internal signals for debugging
    initial begin
        $monitor("Time=%t load=%b done=%b sdo=%b plaintext_internal=%h", 
                 $time, load, done_decrypt, sdo, dut.plaintext);
    end

    // Create dumpfile
    initial begin
        $dumpfile("testbench_aes_spi.vcd");
        $dumpvars(0, testbench_aes_spi);
    end

    // generate internal clock for aes_core
    always begin
        clk = 1'b0; #5;
        clk = 1'b1; #5;
    end

    // Initialize
    initial begin
        sck = 0;
        sdi = 0;
        load = 0;
        #100;

        // TEST 1
        $display("\n");
        $display("Starting Test 1...");
        $display("\n");
        key        = 128'h2B7E151628AED2A6ABF7158809CF4F3C;
        cyphertext = 128'h3925841D02DC09FBDC118597196A0B32;
        expected   = 128'h3243F6A8885A308D313198A2E0370734;
        
        data_in = {cyphertext, key};
        
        // Assert load
        $display("Asserting load signal");
        load = 1;
        #20;
        
        // Send 256 bits via SPI (cyphertext first, then key)
        $display("Sending 256 bits via SPI...");
        for (int i = 0; i < 256; i++) begin
            sdi = data_in[255 - i];
            #10; sck = 1;
            #10; sck = 0;
        end
        $display("SPI input complete");
        
        // Deassert load
        #20;
        $display("Deasserting load signal");
        load = 0;
        #20;
        
        // Wait for done_decrypt
        $display("Waiting for decryption to complete...");
        wait(done_decrypt == 1);
        $display("Decryption complete at time %t", $time);
        $display("Internal plaintext = %h", dut.plaintext);
        $display("SPI plaintext_captured = %h", dut.spi.plaintextcaptured);
        $display("SPI wasdone = %b", dut.spi.wasdone);
        
        // Small delay for output to stabilize
        #100;
        
// Read out 128 bits of plaintext
        $display("Reading 128 bits from SPI...");
        $display("First few bits:");
        for (int i = 0; i < 128; i++) begin
            #10; 
            sck = 1;      // Drive Clock High
            #10; 
            sck = 0;      // Drive Clock Low (DUT updates sdo here)
            #5; 
            data_out[127 - i] = sdo; // Sample STABLE data
            if (i < 16) $display("  Bit %3d: sdo=%b", i, sdo);
            #5;
        end
        
        plaintext = data_out;
        #50;
        
        // Check result
        $display("\n");
        $display("TEST 1 RESULTS:");
        $display("plaintext = %h", plaintext);
        $display("expected  = %h", expected);
        $display("\n");
        
        if (plaintext === 128'bx) begin
            $display("ERROR: plaintext is X (undefined)");
            pass = 0;
        end else if (plaintext == expected) begin
            $display("Test 1 PASSED");
        end else begin
            $display("Test 1 FAILED");
            pass = 0;
        end

        // ===== TEST 2 =====
        #200;
        $display("");
        $display("\n");
        $display("Starting Test 2...");
        $display("\n");
        key        = 128'h000102030405060708090A0B0C0D0E0F;
        cyphertext = 128'h69C4E0D86A7B0430D8CDB78070B4C55A;
        expected   = 128'h00112233445566778899AABBCCDDEEFF;
        
        data_in = {cyphertext, key};
        
        // Assert load
        $display("Asserting load signal");
        load = 1;
        #20;
        
        // Send 256 bits via SPI
        $display("Sending 256 bits via SPI...");
        for (int i = 0; i < 256; i++) begin
            sdi = data_in[255 - i];
            #10; sck = 1;
            #10; sck = 0;
        end
        $display("SPI input complete");
        
        // Deassert load
        #20;
        $display("Deasserting load signal");
        load = 0;
        #20;
        
        // Wait for done_decrypt
        $display("Waiting for decryption to complete...");
        wait(done_decrypt == 1);
        $display("Decryption complete at time %t", $time);
        $display("Internal plaintext = %h", dut.plaintext);
        $display("SPI plaintext_captured = %h", dut.spi.plaintextcaptured);
        $display("SPI wasdone = %b", dut.spi.wasdone);
        
        // Small delay for output to stabilize
        #100;
        
        // Read out 128 bits of plaintext
        $display("Reading 128 bits from SPI...");
        $display("First few bits:");
        for (int i = 0; i < 128; i++) begin
            #10; sck = 1;
            #5;
            data_out[127 - i] = sdo;
            if (i < 16) $display("  Bit %3d: sdo=%b", i, sdo);
            #5; sck = 0;
            #5;
        end
        
        plaintext = data_out;
        #50;
        
        // Check result
        $display("\n");
        $display("TEST 2 RESULTS:");
        $display("plaintext = %h", plaintext);
        $display("expected  = %h", expected);
        $display("\n");
        
        if (plaintext === 128'bx) begin
            $display("ERROR: plaintext is X (undefined)");
            pass = 0;
        end else if (plaintext == expected) begin
            $display("Test 2 PASSED");
        end else begin
            $display("Test 2 FAILED");
            pass = 0;
        end

        // ===== FINAL RESULTS =====
        #100;
        $display("");
        if (pass) begin
            $display("\n");
            $display("ALL TESTS PASSED!");
            $display("\n");
        end else begin
            $display("\n");
            $display("ERROR: One or more tests failed");
            $display("\n");
        end

        $stop();
    end
endmodule