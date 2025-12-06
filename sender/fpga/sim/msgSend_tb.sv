`timescale 1ns/1ns
// Josaphat Ngoga
// jngoga@g.hmc.edu
// Modified: 12/4/2025

/////////////////////////////////////////////
// msgSend_tb()
//      Enhanced testbench for msgSend with data verification
//      Tests sending both ciphertext and key and track all signals bit by bit
/////////////////////////////////////////////

module msgSend_tb();

    // Parameters
    localparam CLK_FREQ = 48_000_000;
    localparam TX_FREQ  = 1_000_000; // Faster for testing

    // Signals
    logic clk, reset, start;
    logic [7:0] msg_out;
    logic tx_clk, send_done;

    // DUT inputs
    logic [127:0] ciphertext = 128'hDEADBEEF_F00DBABE_12345678_ABCDEF00;
    logic [127:0] key        = 128'h0123456789ABCDEF_FEDCBA98_76543210;

    // Expected data array (32 bytes)
    logic [7:0] expected_data [0:31];
    logic [7:0] received_data [0:31];
    integer byte_count;
    logic prev_tx_clk;
    logic tx_clk_rising;

    // Instantiate DUT
    msgSend #(
        .CLK_FREQ(CLK_FREQ),
        .TX_FREQ(TX_FREQ)
    ) dut (
        .clk(clk),
        .reset(reset),
        .start(start),
        .ciphertext(ciphertext),
        .key(key),
        .msg_out(msg_out),
        .tx_clk(tx_clk),
        .send_done(send_done)
    );

    // Clock generation
    always #10 clk = ~clk;

    // Detect tx_clk rising edge
    always_ff @(posedge clk or negedge reset) begin
        if (!reset)
            prev_tx_clk <= 0;
        else
            prev_tx_clk <= tx_clk;
    end
    assign tx_clk_rising = tx_clk && !prev_tx_clk;

    // Initialize expected data array
    initial begin
        // Ciphertext bytes (MSB first)
        expected_data[0]  = 8'hDE;
        expected_data[1]  = 8'hAD;
        expected_data[2]  = 8'hBE;
        expected_data[3]  = 8'hEF;
        expected_data[4]  = 8'hF0;
        expected_data[5]  = 8'h0D;
        expected_data[6]  = 8'hBA;
        expected_data[7]  = 8'hBE;
        expected_data[8]  = 8'h12;
        expected_data[9]  = 8'h34;
        expected_data[10] = 8'h56;
        expected_data[11] = 8'h78;
        expected_data[12] = 8'hAB;
        expected_data[13] = 8'hCD;
        expected_data[14] = 8'hEF;
        expected_data[15] = 8'h00;
        
        // Key bytes (MSB first)
        expected_data[16] = 8'h01;
        expected_data[17] = 8'h23;
        expected_data[18] = 8'h45;
        expected_data[19] = 8'h67;
        expected_data[20] = 8'h89;
        expected_data[21] = 8'hAB;
        expected_data[22] = 8'hCD;
        expected_data[23] = 8'hEF;
        expected_data[24] = 8'hFE;
        expected_data[25] = 8'hDC;
        expected_data[26] = 8'hBA;
        expected_data[27] = 8'h98;
        expected_data[28] = 8'h76;
        expected_data[29] = 8'h54;
        expected_data[30] = 8'h32;
        expected_data[31] = 8'h10;
    end

    // Capture transmitted data when tx_clk is high
    logic prev_tx_clk_for_capture;
    
    always @(posedge clk) begin
        if (!reset) begin
            byte_count <= 0;
            prev_tx_clk_for_capture <= 0;
        end else begin
            prev_tx_clk_for_capture <= tx_clk;
            
            // Capture on rising edge of tx_clk
            if (tx_clk && !prev_tx_clk_for_capture && byte_count < 32) begin
                // Sample msg_out
                #1; 
                received_data[byte_count] = msg_out;
                $display("[%0t ns] Byte %0d: Sent=0x%h, Expected=0x%h %s",
                         $time, byte_count, msg_out, expected_data[byte_count],
                         (msg_out == expected_data[byte_count]) ? "✓ PASS" : "✗ FAIL");
                byte_count <= byte_count + 1;
            end
        end
    end

    // Stimulus
    initial begin
        clk   = 0;
        reset = 0;
        start = 0;
        byte_count = 0;
        
        $display("\n\n");
        $display("msgSend Testbench - Ciphertext + Key");
        $display("\n");
        $display("Ciphertext: 0x%h", ciphertext);
        $display("Key:        0x%h", key);
        $display("\n\n");
        
        #100;
        reset = 1;
        #100;
        
        $display("[%0t ns] Starting transmission...\n", $time);
        start = 1;
        #40;
        start = 0;

        wait(send_done);
        $display("\n[%0t ns] Transmission complete (send_done asserted)", $time);
        
        // Verify all data
        #200;
        $display("\n\n");
        $display("Verification Summary");
        $display("\n");
        verify_transmission();
        $display("\n\n");
        
        #100;
        $stop;
    end

    // Verification task
    task verify_transmission();
        integer i;
        integer errors;
        errors = 0;
        
        $display("Checking %0d bytes transmitted...\n", byte_count);
        
        for (i = 0; i < byte_count; i = i + 1) begin
            if (received_data[i] !== expected_data[i]) begin
                $display("ERROR at byte %0d: Expected 0x%h, Got 0x%h",
                         i, expected_data[i], received_data[i]);
                errors = errors + 1;
            end
        end
        
        if (byte_count != 32) begin
            $display("ERROR: Expected 32 bytes, but received %0d bytes", byte_count);
            errors = errors + 1;
        end
        
        $display("");
        if (errors == 0) begin
            $display("*** ALL TESTS PASSED ***");
            $display("Successfully transmitted 32 bytes (16 ciphertext + 16 key)");
        end else begin
            $display("*** %0d TEST(S) FAILED ***", errors);
        end
    endtask

    // Monitor key signals (less verbose)
    initial begin
        $display("Time\t\ttx_clk\tmsg_out\tsend_done");
        $display("\n");
    end
    
    always @(posedge clk) begin
        if (tx_clk_rising || send_done)
            $display("%0t ns\t%b\t0x%h\t%b", $time, tx_clk, msg_out, send_done);
    end

endmodule