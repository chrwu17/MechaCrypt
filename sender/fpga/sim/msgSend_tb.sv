`timescale 1ns/1ns
// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

/////////////////////////////////////////////
// msgSend_tb()
//      Testbench for ability to split array and send bytes out one at a time
/////////////////////////////////////////////

module msgSend_tb();

    // Parameters
    localparam CLK_FREQ = 48_000_000;
    localparam TX_FREQ  = 10;

    // Signals
    logic clk, reset, start;
    logic [7:0] msg_out;
    logic tx_clk, send_done;

    // DUT input
    logic [127:0] msg_in = 128'hDEADBEEF_F00DBABE_12345678_ABCDEF00;

    // Instantiate DUT
    msg_send #(
        .CLK_FREQ(CLK_FREQ),
        .TX_FREQ(TX_FREQ)
    ) dut (
        .clk(clk),
        .reset(reset),
        .start(start),
        .msg_in(msg_in),
        .msg_out(msg_out),
        .tx_clk(tx_clk),
        .send_done(send_done)
    );

    // Clock generation
    always #10 clk = ~clk;

    // Stimulus
    initial begin
        clk   = 0;
        reset = 0;
        start = 0;
        #100;
        reset = 1;
        #100;
        start = 1;
        #20;
        start = 0;

        wait(send_done);
        $display("[%0t ns] send_done", $time);
        #200;
        $stop;
    end

    // Monitor signals
    initial begin
        $monitor("[%0t ns] tx_clk=%b msg_out=%h send_done=%b",
                  $time, tx_clk, msg_out, send_done);
    end
endmodule
