`timescale 1ns/1ns
// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

////////////////////////////////////////////////////////////
// spram_RW_tb.sv() 
//      Testbench to verify SPRAM write and read functionality.
//      Writes a known ciphertext to SPRAM and reads it back to verify correctness.
////////////////////////////////////////////////////////////

module spram_RW_tb;
    // DUT signals
    logic         clk;
    logic         done;          
    logic [127:0] ciphertext;
    logic [127:0] cipher_out;    
    logic [13:0]  baseAddr;
    logic         write_done;
    logic         read_done;

    // generate clock
    always begin
      clk = 1'b0; #5;
      clk = 1'b1; #5;
    end

    // Instantiate Write DUT
    spramWrite u_writer (
        .clk        (clk),
        .done       (done),
        .ciphertext (ciphertext),
        .baseAddr   (baseAddr),
        .write_done (write_done)
    );

    // Instantiate Read DUT
    spramRead u_reader (
        .clk        (clk),
        .write_done (write_done),
        .baseAddr   (baseAddr),
        .cipher_out (cipher_out),
        .read_done  (read_done)
    );

    // Testbench control
    initial begin
        // Initialize signals
        done = 0;
        ciphertext = 128'hDEADBEEF_F00DBABE_12345678_ABCDEF00;
        #25;

        // Impluse done to start write
        $display("[%0t] AES done asserted, starting write...", $time);
        done <= 1;
        #10;
        done <= 0;

        // Wait for write
        wait (write_done == 1);
        $display("[%0t] Write complete. baseAddr=%0d", $time, baseAddr);

        // Wait for read
        wait (read_done == 1);
        $display("[%0t] Read complete.", $time);

        // Compare results
        if (cipher_out === ciphertext)
            $display("All tests pass!");
            $display("Expected: %h", ciphertext);
            $display("Received: %h", cipher_out);
        else begin
            $error("Testbench failed!");
            $display("Expected: %h", ciphertext);
            $display("Received: %h", cipher_out);
        end

        $finish;
    end
endmodule
