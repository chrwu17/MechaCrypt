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
    
    aes_core dut(clk, load, key, cyphertext, done_decrypt, plaintext);
    
    always begin
        clk = 1'b0; #5;
        clk = 1'b1; #5;
    end
        
    initial begin
        load       = 0;
        key        = 0;
        cyphertext = 0;
        
        #20;

        // Test case from FIPS-197
        $display("Starting Test 1...");
        key        = 128'h2B7E151628AED2A6ABF7158809CF4F3C;
        cyphertext = 128'h3925841D02DC09FBDC118597196A0B32;
        expected   = 128'h3243F6A8885A308D313198A2E0370734;
        
        @(posedge clk);
        load = 1'b1; 
        @(posedge clk);
        @(posedge clk);
        @(posedge clk);
        load = 1'b0;

        // Monitor progress
        $display("Waiting for decryption...");
        fork
            begin
                wait (done_decrypt == 1'b1);
                $display("done_decrypt asserted at time %t", $time);
            end
            begin
                #50000; // Timeout
                $display("ERROR: Timeout waiting for done_decrypt");
                $stop();
            end
        join_any
        
        // Give extra time for plaintext to settle
        repeat(10) @(posedge clk);
        
        $display("plaintext = %h", plaintext);
        $display("expected  = %h", expected);

        if (plaintext === 128'bx) begin
            $display("ERROR: plaintext is X (undefined)");
            pass = 0;
        end else if (plaintext == expected) begin
            $display("Test 1 PASSED");
        end else begin
            $display("Test 1 FAILED");
            pass = 0;
        end
         
        #100;
        $stop();
    end
endmodule
