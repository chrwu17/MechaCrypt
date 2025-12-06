// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/22/2025

`timescale 10ns/1ns

/////////////////////////////////////////////
// inv_mixcolumns_sync_tb
// Tests inv_mixcolumns_sync module with 1-cycle latency
/////////////////////////////////////////////

module inv_mixcolumns_sync_tb();
    logic clk;
    logic [127:0] a, y, yExpected;
    logic pass = 1'b1;

    // Device Under Test
    inv_mixcolumns_sync dut(clk, a, y);

    // Generate clock
    always begin
        clk = 1'b0; #5;
        clk = 1'b1; #5;
    end

    initial begin
        // Initialize
        a = 0;
        #10;

        // Test Case 1 (FIPS-197 Appendix B)
        a <= 128'h63CAB7040953D051CD60E0E7BA70E18C;
        yExpected <= 128'h9DEF7D15AD1FD9B0B5DCD714AA7CA4D5;
        
        // Wait for pipeline: 1 cycle for input register + combinational output
        @(posedge clk);
        @(posedge clk); // Extra cycle for pipeline
        #1;             // Small delay for combinational logic
        
        if (y != yExpected) begin
            $display("Test 1 FAILED: y = %h, expected = %h", y, yExpected);
            pass = 0;
        end else begin
            $display("Test 1 passed");
        end

        // Test Case 2
        a <= 128'h5F72641557F5BC92F7BE3B291188CF26;
        yExpected <= 128'h6353E08C0960E104CD70B751B0558E1B;
        @(posedge clk);
        @(posedge clk);
        #1;
        
        if (y != yExpected) begin
            $display("Test 2 FAILED: y = %h, expected = %h", y, yExpected);
            pass = 0;
        end else begin
            $display("Test 2 passed");
        end

        // Test Case 3
        a <= 128'h4D7E3E5E1E423169FEA531E04F6A7A0B;
        yExpected <= 128'h0183D4053E2C190FEF2F2F6513178EDE;
        @(posedge clk);
        @(posedge clk);
        #1;
        
        if (y != yExpected) begin
            $display("Test 3 FAILED: y = %h, expected = %h", y, yExpected);
            pass = 0;
        end else begin
            $display("Test 3 passed");
        end

        // Test Case 4
        a <= 128'hB458124C68B68A014B99F82E5F61F7E0;
        yExpected <= 128'h8CC955A2A1EFE9F22A95912ACF1E35CD;
        @(posedge clk);
        @(posedge clk);
        #1;
        
        if (y != yExpected) begin
            $display("Test 4 FAILED: y = %h, expected = %h", y, yExpected);
            pass = 0;
        end else begin
            $display("Test 4 passed");
        end

        // Test Case 5
        a <= 128'hF69F2445DF4F9B17AD2B417BE66C3710;
        yExpected <= 128'h09FFCB35137B6F1B0C8F83BC3283A2BE;
        @(posedge clk);
        @(posedge clk);
        #1;
        
        if (y != yExpected) begin
            $display("Test 5 FAILED: y = %h, expected = %h", y, yExpected);
            pass = 0;
        end else begin
            $display("Test 5 passed");
        end

        #20;

        // Check results
        if (pass)
            $display("All InvMixColumns pipelined test cases passed.");
        else
            $display("ERROR: Some tests failed");

        $stop();
    end
endmodule
