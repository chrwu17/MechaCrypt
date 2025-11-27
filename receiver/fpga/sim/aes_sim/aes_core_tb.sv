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
    logic pass = 1'b1; // indicates if test passed
    
    // device under test
    aes_core dut(clk, load, key, cyphertext, done_decrypt, plaintext);
    
    // generate clock and load signals
    always begin
			clk = 1'b0; #5;
			clk = 1'b1; #5;
	end
        
    // test cases
    initial begin
        load       = 0;
        key        = 0;
        cyphertext = 0;

        // Test case from FIPS-197 Appendix A.1, B
        key        <= 128'h2B7E151628AED2A6ABF7158809CF4F3C;
        cyphertext <= 128'h3925841D02DC09FBDC118597196A0B32;
        expected   <= 128'h3243F6A8885A308D313198A2E0370734;
        
        //Pulse load to start conversion
        @(posedge clk); load = 1;
        @(posedge clk); load = 0;

        // wait for done_decrypt signal
        wait (done_decrypt==1); #5;
        pass &= (plaintext == expected); #10;

        // Alternate test case from Appendix C.1
        key        <= 128'h000102030405060708090A0B0C0D0E0F;
        cyphertext <= 128'h69C4E0D86A7B0430D8CDB78070B4C55A;
        expected   <= 128'h00112233445566778899AABBCCDDEEFF;

        // load = 1'b1; #22; load = 1'b0; //Pulse load to start conversion
        //Pulse load to start conversion
        @(posedge clk); load = 1;
        @(posedge clk); load = 0;

        // wait for done_decrypt signal
        wait (done_decrypt==1); #5;
        pass &= (plaintext == expected); #50; #50;

        // Check results
        if (pass)
            $display("All tests passed");
        else
            $display("Error: one or more tests failed");

        $stop();
    end
    
endmodule