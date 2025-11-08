`timescale 1ns/1ns
// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

/////////////////////////////////////////////
// msg_send_tb()
// Testbench for ability to receive bytes one at a time and reconstruct message array
/////////////////////////////////////////////

module msg_receive_tb();

    // Parameters
    localparam integer TOTAL_BYTES   = 16;

    // Signals
    logic clk, reset, tx_clk;
    logic [7:0] data_in;
    logic [127:0] msg_out;
    logic done;

    // Reference message
    logic [127:0] expected_data = 128'hDEADBEEF_F00DBABE_12345678_ABCDEF00;

    // Instantiate DUT
    msg_receive #(
        .TOTAL_BYTES(TOTAL_BYTES)
    ) dut (
        .clk(clk),
        .reset(reset),
        .tx_clk(tx_clk),
        .data_in(data_in),
        .msg_out(msg_out),
        .done(done)
    );

    // Generate clock
    always #10 clk = ~clk;

    // Stimulus
    integer i;
    logic [7:0] test_bytes [0:15];

    initial begin
        // Split the 128-bit word into 16 bytes for convenience
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
    end

    initial begin
        clk   = 0;
        tx_clk = 0;
        reset = 0;
        data_in = 8'h00;
        #100;
        reset = 1;
        #100;

        // Send bytes and toggle tx_clk
        for (i = 0; i < TOTAL_BYTES; i = i + 1) begin
            data_in = test_bytes[i];

            #5000 tx_clk = 1;
            #5000 tx_clk = 0;
        end

        // Wait for done signal
        wait(done);

        // Show result
        $display("[%0t ns] Done.", $time);
        $display("Expected = %032h", expected_data);
        $display("Received = %032h", msg_out);

        if (msg_out === expected_data)
            $display("Data matches expected message.");
        else
            $display("Data mismatch.");

        #1000;
        $stop;
    end

    // Monitor signals
    initial begin
        $monitor("[%0t ns] tx_clk=%b data_in=%h done=%b", 
                  $time, tx_clk, data_in, done);
    end

endmodule
