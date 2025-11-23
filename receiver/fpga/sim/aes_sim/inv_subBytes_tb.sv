// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/22/2025

`timescale 10ns/1ns

/////////////////////////////////////////////
// testbench_inv_subBytes_tb
// Tests inv_subBytes module for proper inverse byte substitution
/////////////////////////////////////////////

module inv_subBytes_tb();
    logic clk;
    logic [127:0] a, y, yExpected;
    logic pass = 1'b1; // indicates if test passed

    // Device Under Test
    inv_subBytes dut(clk, a, y);

    // Generate clock
    always begin
        clk = 1'b0; #5;
        clk = 1'b1; #5;
    end

    // Test inverse byte substitution using examples from FIPS-197 Appendix
    initial begin
        // Test Case 1
        a <= 128'h231A42C2C4BE045DC7C7463AE19AC518; // ciphertext-state before InvSubBytes
        yExpected <= 128'h3243F6A8885A308D313198A2E0370734; // original plaintext-state
        @(posedge clk); #1; // Delay to account for S-box latency
        pass &= (y == yExpected);

        // Test Case 2
        a <= 128'h638293C31BFC33F5C4EEACEA4BC12816; // after SubBytes
        yExpected <= 128'h00112233445566778899AABBCCDDEEFF; // before SubBytes
        @(posedge clk); #1;
        pass &= (y == yExpected);

        // Test Case 3
        a <= 128'h7A89BCA0EAFE32108D4F6BDC7CBE5D2F;
        yExpected <= 128'hBDF27847BB0CA17CB4920593015A8D4E;
        @(posedge clk); #1;
        pass &= (y == yExpected);

        // Test Case 4
        a <= 128'h5086CB9BA5E64F4A73A5E3FBAE8C7D6D;

        yExpected <= 128'h6CDC59E829F5925C8F294D63BEF013B3;
        @(posedge clk); #1;
        pass &= (y == yExpected);

        // Test Case 5
        a <= 128'hD0C9E1B6F0F6B1BE8C1A6EAC8A1FA6B4;
        yExpected <= 128'h6012E07917D6565AF04345AACFCBC5C6;
        @(posedge clk); #1;
        pass &= (y == yExpected);

        // Check results
        if (pass)
            $display("All test cases ran succesfully");
        else
            $display("Error: y = %h, expected %h", y, yExpected);
        $stop();
    end
endmodule
