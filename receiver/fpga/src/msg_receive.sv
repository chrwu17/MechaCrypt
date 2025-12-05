// Josaphat Ngoga
// jngoga@g.hmc.edu
// 11/6/2025

//////////////////////////////////////////////////////////// 
// msg_receive() 
//    Receives 8-bit data bytes synchronized to a slower transfer clock (tx_clk) from the sender module,
//    and reconstructs them into a 128-bit output message (16 bytes).
//    Added debug outputs to diagnose reception issues.
////////////////////////////////////////////////////////////

module msg_receive #(
    parameter TOTAL_BYTES = 16,
    // FIX FOR 3 HZ INPUT: Increased debounce cycle count significantly.
    // 2,400,000 cycles at 24 MHz clock = 100 milliseconds lockout time.
    // This is robust against mechanical bounce but safe (less than 333ms period of a 3Hz signal).
    parameter DEBOUNCE_CYCLES = 1_200_000
    )(
    // input  logic          clk,
    input  logic          reset,
    input  logic          tx_clk,
    input  logic [7:0]    data_in,
    
    // SPI interface to MCU
    input  logic          sck,
    input  logic          cs,
    output logic          sdo,
    
    // Debug outputs - connect to LEDs or test points
    output logic [3:0]    debug_byte_count, // Shows how many bytes received (0-15)
    output logic          debug_tx_activity, // Blinks when tx_clk edges detected
    output logic          debug_debounce_locked, // Shows when debounce is blocking
    output logic          debug_tx_clk_raw,  // Shows raw tx_clk input state
    output logic          debug_tx_clk_synced, // Shows synchronized tx_clk
    
    output logic          done,
    output logic          ready);

    logic clk;
    HSOSC #(.CLKHF_DIV(2'b11)) 
          hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk)); // 24 MHz
    // Sync tx_clk to FPGA clk domain with stable initial state
    logic tx_clk_sync_0, tx_clk_sync_1, tx_clk_prev;
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            tx_clk_sync_0 <= 0;
            tx_clk_sync_1 <= 0;
            tx_clk_prev <= 0;  // Track previous state
        end else begin
            tx_clk_sync_0 <= tx_clk;
            tx_clk_sync_1 <= tx_clk_sync_0;
            tx_clk_prev <= tx_clk_sync_1;  // Save previous for edge detection
        end
    end

    // Detect rising edge - now uses stable previous state
    logic tx_clk_rise;
    assign tx_clk_rise = (tx_clk_sync_1 && !tx_clk_prev);

    // ========== Debouncing Logic ==========
    logic [$clog2(DEBOUNCE_CYCLES)-1:0] debounce_counter;
    logic tx_clk_valid;
    logic tx_clk_rise_debounced;
    logic [7:0] startup_delay;  // Ignore first ~256 cycles after reset

    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            debounce_counter <= 0;
            tx_clk_valid <= 1'b0;  // Start INVALID until startup completes
            startup_delay <= 8'd255;
        end else begin
            // Startup delay counter
            if (startup_delay > 0) begin
                startup_delay <= startup_delay - 1;
                tx_clk_valid <= 1'b0;
            end
            // Normal debounce operation
            else if (debounce_counter > 0) begin
                debounce_counter <= debounce_counter - 1;
                tx_clk_valid <= 1'b0;
            end else begin
                tx_clk_valid <= 1'b1;
            end
            
            if (tx_clk_rise && tx_clk_valid) begin
                debounce_counter <= DEBOUNCE_CYCLES;
            end
        end
    end

    assign tx_clk_rise_debounced = tx_clk_rise && tx_clk_valid;

    // ========== Byte Assembly Logic ==========
    logic [4:0] idx;
    logic [127:0] buffer;
    logic receiving;

    always_ff @(posedge clk or negedge reset) begin
		if (!reset) begin
			idx        <= 0;
			buffer     <= 0;
			receiving  <= 0;
			done       <= 0;
		end 
		else begin
			// Only process edges if we haven't completed yet
			if (tx_clk_rise_debounced && !done) begin
				receiving <= 1'b1;
				buffer <= {buffer[119:0], data_in};

				if (idx == (TOTAL_BYTES - 1)) begin
					done       <= 1'b1;
					receiving  <= 1'b0;
					idx        <= 0;
				end else begin
					idx        <= idx + 1;
				end
			end
			
			// Only clear when SPI transaction completes
			if (cs_rise && sending) begin
				done <= 1'b0;
				idx <= 0;  // Reset index too
			end
		end
	end

    // ========== Debug Signal Generation ==========
    // Blink LED briefly when any tx_clk edge detected (debounced or not)
    logic [15:0] activity_counter;
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            activity_counter <= 0;
        end else begin
            if (tx_clk_rise) begin
                activity_counter <= 16'hFFFF;  // Keep LED on for ~1ms
            end else if (activity_counter > 0) begin
                activity_counter <= activity_counter - 1;
            end
        end
    end
    
    assign debug_tx_activity = (activity_counter > 0);
    assign debug_byte_count = idx;  // Current byte count
    assign debug_debounce_locked = !tx_clk_valid;  // HIGH when debounce blocking
    assign debug_tx_clk_raw = tx_clk;  // Raw input
    assign debug_tx_clk_synced = tx_clk_sync_1;  // After synchronization

    // ========== SPI Output Logic ==========
    logic [127:0] shiftOut;
    logic         sending;
    logic         sdo_next;
    logic         done_prev;
    
    // Synchronize sck to clk domain
    logic sck_sync_0, sck_sync_1, sck_prev;
    logic sck_rise, sck_fall;
    
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            sck_sync_0 <= 1'b0;
            sck_sync_1 <= 1'b0;
            sck_prev   <= 1'b0;
        end else begin
            sck_sync_0 <= sck;
            sck_sync_1 <= sck_sync_0;
            sck_prev   <= sck_sync_1;
        end
    end
    
    assign sck_rise = sck_sync_1 && !sck_prev;
    assign sck_fall = !sck_sync_1 && sck_prev;

    // Synchronize cs to clk domain
    logic cs_sync_0, cs_sync_1, cs_prev;
    logic cs_rise;
    
    always_ff @(posedge clk or negedge reset) begin
        if (!reset) begin
            cs_sync_0 <= 1'b1;
            cs_sync_1 <= 1'b1;
            cs_prev   <= 1'b1;
        end else begin
            cs_sync_0 <= cs;
            cs_sync_1 <= cs_sync_0;
            cs_prev   <= cs_sync_1;
        end
    end
    
    assign cs_rise = cs_sync_1 && !cs_prev;

    // Combined SPI logic - all driven from clk domain
    // Keep your sck_rise detection as is
	assign sck_rise = sck_sync_1 && !sck_prev;

	// Add falling edge detection
	logic sck_fall;
	assign sck_fall = !sck_sync_1 && sck_prev;

	// Change shift logic to use falling edge:
	always_ff @(posedge clk or negedge reset) begin
		if (!reset) begin
			done_prev <= 1'b0;
			sending   <= 1'b0;
			shiftOut  <= 128'd0;
			sdo_next  <= 1'b0;
		end 
		else begin
			done_prev <= done;
			
			if (done && !done_prev) begin
				sending  <= 1'b1;
				shiftOut <= buffer;
				sdo_next <= buffer[127];
			end
			
			if (cs_rise && sending) begin
				sending  <= 1'b0;
				sdo_next <= 1'b0;
			end
			
			// CHANGED: Use falling edge to shift data
			if (sck_fall && sending && !cs_sync_1) begin
				shiftOut <= {shiftOut[126:0], 1'b0};
				sdo_next <= shiftOut[126];
			end
		end
	end

    assign sdo = sdo_next;
    assign ready = sending;
endmodule