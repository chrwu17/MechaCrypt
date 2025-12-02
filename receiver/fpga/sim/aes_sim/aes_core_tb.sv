// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/26/2025

`timescale 10ns/1ns

/////////////////////////////////////////////
// testbench_aes_core
//  Tests AES decryption with cases from FIPS-197 appendix obtained by
//  switching the plaintext and ciphertext from encryption examples.
//  The testbench evaluates the correct implementation of Equivalent Inverse Cipher algorithm
/////////////////////////////////////////////

module testbench_aes_core();
    logic clk, load, done_decrypt;
    logic [127:0] key, plaintext, cyphertext, expected;
    logic pass = 1'b1;
    
    // device under test
    aes_core dut(clk, load, key, cyphertext, done_decrypt, plaintext);
    
    // generate clock
    always begin
        clk = 1'b0; #5;
        clk = 1'b1; #5;
    end
        
    // test cases
    initial begin
        load       = 0;
        key        = 0;
        cyphertext = 0;
        
        #20; // Initial delay

        // Test case from FIPS-197 Appendix A.1, B
        key        = 128'h2B7E151628AED2A6ABF7158809CF4F3C;
        cyphertext = 128'h3925841D02DC09FBDC118597196A0B32;
        expected   = 128'h3243F6A8885A308D313198A2E0370734;
        
        @(posedge clk);
        load = 1'b1; 
        @(posedge clk);
        @(posedge clk);
        load = 1'b0;

        // wait for done_decrypt signal (will take longer now due to pipelining)
        wait (done_decrypt == 1'b1);
        #15;

        if (plaintext == expected) begin
            $display("Test 1 PASSED");
        end else begin
            $display("Test 1 FAILED: plaintext = %h, expected %h", plaintext, expected);
            pass = 0;
        end
         
        #50;

        // Test case from Appendix C.1
        key        = 128'h000102030405060708090A0B0C0D0E0F;
        cyphertext = 128'h69C4E0D86A7B0430D8CDB78070B4C55A;
        expected   = 128'h00112233445566778899AABBCCDDEEFF;

        @(posedge clk);
        load = 1'b1;
        @(posedge clk);
        @(posedge clk);
        load = 1'b0;

        // wait for done_decrypt signal
        wait (done_decrypt == 1'b1);
        #15;

        if (plaintext == expected) begin
            $display("Test 2 PASSED");
        end else begin
            $display("Test 2 FAILED: plaintext = %h, expected %h", plaintext, expected);
            pass = 0;
        end 
        
        #50;

        // Check results
        if (pass)
            $display("All tests passed");
        else
      	    $display("Error: one or more tests failed");

        $stop();

        $stop();
    end
    
endmodule