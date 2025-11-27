// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/26/2025

`timescale 10ns/1ns

/////////////////////////////////////////////
// testbench_inv_shiftRows_tb
// Tests inverse shiftRows module for proper shifting operations.
/////////////////////////////////////////////

module inv_shiftRows_tb();
    logic [127:0] a, y, yExpected;
    logic pass = 1'b1;

    // DUT
    inv_shiftRows dut(a, y);

    initial begin
        // Test 1 
        a = 128'h3243F6A8885A308D313198A2E0370734;
        yExpected = 128'h3237988D884307A2315AF634E03130A8;
        #1; pass &= (y == yExpected);

        // Test 2 
        a = 128'h00010203101112132021222330313233;
        yExpected = 128'h00312213100132232011023330211203;
        #1; pass &= (y == yExpected);

        // Test 3 
        a = 128'h00112233445566778899AABBCCDDEEFF;
        yExpected = 128'h00DDAA774411EEBB885522FFCC996633;
        #1; pass &= (y == yExpected);

        // Test 4 
        a = 128'hFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF;
        yExpected = 128'hFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF;
        #1; pass &= (y == yExpected);

        // Test 5 
        a = 128'h00000000000000000000000000000000;
        yExpected = 128'h00000000000000000000000000000000;
        #1; pass &= (y == yExpected);

        // Test 6 
        a = 128'h1A2B3C4D5E6F708192A3B4C5D6E7F809;
        yExpected = 128'h1AE7B4815E2BF8C5926F3C09D6A3704D;
        #1; pass &= (y == yExpected);

        // Check results
        if (pass)
            $display("All inverse shiftRows tests passed!");
        else
            $display("ERROR: y = %h, expected = %h", y, yExpected);

        $stop();
    end
endmodule

