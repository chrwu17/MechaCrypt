// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/22/2025

`timescale 10ns/1ns

/////////////////////////////////////////////
// testbench_inv_mixcolumns_tb
// Tests inv_mixcolumns module for column transformation as well as the
// functionality and implementation of the extended galois multiplication math.
/////////////////////////////////////////////

module inv_mixcolumns_tb();
    logic [127:0] a, y, yExpected;
    logic pass = 1'b1;

    // Device Under Test
    inv_mixcolumns dut(a, y);

    initial begin
        // Test Case 1 (FIPS-197 Appendix B)
        a <= 128'h63CAB7040953D051CD60E0E7BA70E18C;   // After MixColumns (Round 1)
        yExpected <= 128'h9DEF7D15AD1FD9B0B5DCD714AA7CA4D5; // Before MixColumns
        #1;
        pass &= (y == yExpected);

        // Test Case 2
        a <= 128'h5F72641557F5BC92F7BE3B291188CF26;
        yExpected <= 128'h6353E08C0960E104CD70B751B0558E1B;
        #1;
        pass &= (y == yExpected);

        // Test Case 3
        a <= 128'h4D7E3E5E1E423169FEA531E04F6A7A0B;
        yExpected <= 128'h0183D4053E2C190FEF2F2F6513178EDE;
        #1;
        pass &= (y == yExpected);

        // Test Case 4
        a <= 128'hB458124C68B68A014B99F82E5F61F7E0;
        yExpected <= 128'h8CC955A2A1EFE9F22A95912ACF1E35CD;
        #1;
        pass &= (y == yExpected);

        // Test Case 5
        a <= 128'hF69F2445DF4F9B17AD2B417BE66C3710;
        yExpected <= 128'h9FFCB35137B6F1B0C8F83BC3283A2BE;
        #1;
        pass &= (y == yExpected);

        // Check results
        if (pass)
            $display("All InvMixColumns test cases passed.");
        else
            $display("ERROR: y = %h, expected = %h", y, yExpected);

        $stop();
    end
endmodule
