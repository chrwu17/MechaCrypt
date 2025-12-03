// Josaphat Ngoga
// jngoga@g.hmc.edu
// 10/15/2025

/////////////////////////////////////////////
// aesDecryption
//   Top level module with SPI interface and SPI core
/////////////////////////////////////////////

module aesDecryption(
           input  logic clk,  // For simulation, replace with internal oscillator in FPGA
           input  logic sck, 
           input  logic sdi,
           input  logic load,
           output logic sdo,
           output logic done_decrypt);
                    
    logic [127:0] key, plaintext, cyphertext;
    // logic clk;
    
    // Synchronize load signal to the clk instead of sck
    logic load_meta, load_sync;
    always_ff @(posedge clk) begin
        load_meta <= load;
        load_sync <= load_meta;
    end

    // Internal high-speed oscillator to generate slow clock
    // HSOSC #(.CLKHF_DIV(2'b00)) 
    //       hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk)); // 48 MHz
            
    aes_spi spi(sck, sdi, sdo, done_decrypt, key, cyphertext, plaintext);

    aes_core core(clk, load_sync, key, cyphertext, done_decrypt, plaintext);
endmodule

/////////////////////////////////////////////
// aes_spi
//   SPI interface.  Shifts in key and cyphertext,
//   Captures plaintext when done, then shifts it out
//   Tricky cases to properly change sdo on negedge clk
/////////////////////////////////////////////

module aes_spi(input  logic sck, 
               input  logic sdi,
               output logic sdo,
               input  logic done_decrypt,
               output logic [127:0] key, cyphertext,
               input  logic [127:0] plaintext);

    logic         sdodelayed, wasdone;
    logic [127:0] plaintextcaptured;
               
    // assert load
    // apply 256 sclks to shift in key and cyphertext, starting with cyphertext[127]
    // then deassert load, wait until done
    // then apply 128 sclks to shift out plaintext, starting with plaintext[127]
    // SPI mode is equivalent to cpol = 0, cpha = 0 since data is sampled on first edge and the first
    // edge is a rising edge (clock going from low in the idle state to high).
    always_ff @(posedge sck)
        if (!wasdone)  {plaintextcaptured, cyphertext, key} = {plaintext, cyphertext[126:0], key, sdi};
        else           {plaintextcaptured, cyphertext, key} = {plaintextcaptured[126:0], cyphertext, key, sdi}; 
    
    // sdo should change on the negative edge of sck
    always_ff @(negedge sck) begin
        wasdone = done_decrypt;
        sdodelayed = plaintextcaptured[126];
    end
    
    // when done_decrypt is first asserted, shift out msb before clock edge
    assign sdo = (done_decrypt & !wasdone) ? plaintext[127] : sdodelayed;
endmodule

/////////////////////////////////////////////
// aes_core
//   AES decryption core
//   Follows SP Network structure similar to encryption
//
//   Decryption follows Equivalent Inverse Cipher algorithm, mimicing encryption datapath
//   for easier hardware reuse and debugging
//   
//   Round keys are generated at the start of decryption and stored since decryption starts with K10
//   and works backwards to K0. 
//   Key schedule is run for all 10 rounds to generate K1..K10, and K0 is the original key.
//   EqInv requires pre-mixing round keys 1..9 by applying InvMixColumns while keeping K0 and K10 unchanged.
//   This is also done during key schedule generation to save time during decryption rounds. It also adds extra
//   cycles due to invmixcolumns pipelining, so the decryption rounds are adjusted accordingly.
/////////////////////////////////////////////

module aes_core(
    input  logic         clk, 
    input  logic         load,
    input  logic [127:0] key, 
    input  logic [127:0] cyphertext,
    output logic         done_decrypt,
    output logic [127:0] plaintext);

    // Internal signals
    logic ka_busy, ka_done;
    logic [3:0][31:0] currKey, nextKey, word;
    logic [31:0] rcon;
    logic [3:0] ka_round, round_idx, roundCount;
    logic [4:0] cycleCount;  // Increased to 5 bits for more cycles
    logic [127:0] state;
    logic [127:0] bfrSub, afterSub, afterShift, afterMix, bfrAdd, afterAdd;
    logic [127:0] keyPreMix, keyAfterMix;

    // Instantiate modules with pipelined versions
    inv_subBytes sub(clk, bfrSub, afterSub);
    inv_shiftRows shift(afterSub, afterShift);
    inv_mixcolumns_sync mix(clk, afterShift, afterMix);
    addRoundKey add(bfrAdd, word, afterAdd);

    getNextKeyEIC keyExp(clk, currKey, rcon, nextKey);  // SYNCHRONOUS

    // Premix keys
    assign keyPreMix = {nextKey[3], nextKey[2], nextKey[1], nextKey[0]};
    inv_mixcolumns_sync premixKeys(clk, keyPreMix, keyAfterMix);

    // Arrays to store keys
    logic [10:0][127:0] roundKeys, premixedKeys;

    always_ff @(posedge clk) begin
        if (load) begin
            roundCount <= 10;
            cycleCount <= 0;
            done_decrypt <= 0;
            plaintext <= 128'h0;    // Initialize plaintext
            state <= 128'h0;        // Initialize state
            bfrSub <= 128'h0;       // Initialize bfrSub

            // Load key and cyphertext
            roundKeys[0] <= {key[127:96], key[95:64], key[63:32], key[31:0]};
            currKey <= {key[127:96], key[95:64], key[63:32], key[31:0]};
            bfrAdd <= cyphertext;

            // Start key schedule
            ka_round <= 0;
            ka_busy <= 1;
            ka_done <= 0;

        end else if (ka_busy) begin
            if (cycleCount == 2) begin
                currKey <= nextKey;
                roundKeys[ka_round + 1] <= {nextKey[3], nextKey[2], nextKey[1], nextKey[0]};
            end
            
            // After single cyle delay for invmixcolumns pipelining, store premixed keys
            if (cycleCount == 4) begin
                if ((ka_round + 1) >= 1 && (ka_round + 1) <= 9) begin
                    premixedKeys[ka_round + 1] <= keyAfterMix;
                end else begin
                    premixedKeys[ka_round + 1] <= {nextKey[3], nextKey[2], nextKey[1], nextKey[0]};
                end
            end

            // Advance to next round
            if (cycleCount == 4) begin
                cycleCount <= 0;
                if (ka_round == 9) begin
                    ka_busy <= 0;
                    ka_done <= 1;
                end else begin
                    ka_round <= ka_round + 1;
                end
            end else begin
                cycleCount <= cycleCount + 1;
            end

        end else if (!done_decrypt & ka_done) begin
            // Initial round (round 10)
            if (roundCount == 10) begin
                if (cycleCount == 0) begin
                    word <= {roundKeys[10][127:96], 
                             roundKeys[10][95:64], 
                             roundKeys[10][63:32], 
                             roundKeys[10][31:0]};
                end 
                if (cycleCount == 4) begin
                    state <= afterAdd;
                end
            end

            // Middle rounds (9 down to 1)
            if ((roundCount > 0) && (roundCount < 10)) begin
                if (cycleCount == 0) begin
                    word <= {premixedKeys[roundCount][127:96], 
                             premixedKeys[roundCount][95:64], 
                             premixedKeys[roundCount][63:32], 
                             premixedKeys[roundCount][31:0]};
                    bfrSub <= state;
                end 
                if (cycleCount == 4) begin
                    bfrAdd <= afterMix;
                end 
                if (cycleCount == 5) begin
                    state <= afterAdd;
                end
            end 

            // Final round (round 0) - no mixcolumns
            if (roundCount == 0) begin
                if (cycleCount == 0) begin
                    word <= {roundKeys[0][127:96], 
                             roundKeys[0][95:64], 
                             roundKeys[0][63:32], 
                             roundKeys[0][31:0]};
                    bfrSub <= state;
                end 
                if (cycleCount == 2) begin
                    bfrAdd <= afterShift; //Skip mixcolumns
                end 
                if (cycleCount == 3) begin
                    plaintext <= afterAdd;
                end
                if (cycleCount == 4) begin
                    done_decrypt <= 1;
                end
            end

            // Update cycle and round counters
            if ((roundCount == 10 && cycleCount == 4) ||
                (roundCount > 0 && roundCount < 10 && cycleCount == 5) ||
                (roundCount == 0 && cycleCount == 4)) begin
                cycleCount <= 0;
                if (roundCount > 0) roundCount <= roundCount - 1;
            end else begin
                cycleCount <= cycleCount + 1;
            end
        end
    end 

    // rcon lookup values for rounds 1-10    
    always_comb begin
        // Ka_round for key schedule advance and roundCount for datapath
        round_idx = (ka_busy) ? ka_round : roundCount;

        case(round_idx)
            4'd0 : rcon = 32'h01000000;
            4'd1 : rcon = 32'h02000000;
            4'd2 : rcon = 32'h04000000;
            4'd3 : rcon = 32'h08000000;
            4'd4 : rcon = 32'h10000000;
            4'd5 : rcon = 32'h20000000;
            4'd6 : rcon = 32'h40000000;
            4'd7 : rcon = 32'h80000000;
            4'd8 : rcon = 32'h1b000000;
            4'd9 : rcon = 32'h36000000;
            default: rcon = 32'h00000000; 
        endcase
    end 
endmodule

///////////////////////// AES Decryption Primitives for Equivalent Inverse Cipher //////////////////////////////

/////////////////////////////////////////////
// sbox
//   Infamous AES byte substitutions with magic numbers
//   Synchronous version which is mapped to embedded block RAMs (EBR)
//   Section 5.1.1, Figure 7
/////////////////////////////////////////////
module sbox_sync(input		logic [7:0] a,
                 input	 	logic       clk,
                 output 	logic [7:0] y);
            
    // sbox implemented as a ROM
    // This module is synchronous and will be inferred using BRAMs (Block RAMs)
    logic [7:0] sbox [0:255];

    initial   $readmemh("D:/MicroPs/MechaCrypt/receiver/fpga/src/sbox.txt", sbox);
    
    	// Synchronous version
    	always_ff @(posedge clk) begin
    		y <= sbox[a];
    	end
endmodule

/////////////////////////////////////////////                  
// inv_sbox_sync
//   Inverse S-box (BRAM, 1-cycle latency)
//   Provided "inv_sbox.txt" as 256-byte ROM (hex)
/////////////////////////////////////////////

module inv_sbox_sync(input  logic [7:0] a,
                     input  logic       clk,
                     output logic [7:0] y);
    
    logic [7:0] invsbox [0:255];
    
    initial $readmemh("D:/MicroPs/MechaCrypt/receiver/fpga/src/inv_sbox.txt", invsbox);
    
    always_ff @(posedge clk) begin
        y <= invsbox[a];
    end
endmodule

/////////////////////////////////////////////                  
// Extended Galois Field Multiplication
//   GF(2^8) multiplication helper functions for AES inverse MixColumns coefficients.
//   They perform polynomial multiplication by x^i for i in {1,2,3} via xtime
//   then combine to get {09, 0B, 0D, 0E} multiplications as shown below:
//        0x09 = x^3 + 1
//        0x0B = x^3 + x + 1
//        0x0D = x^3 + x^2 + 1
//        0x0E = x^3 + x^2 + x
//   and reduction mod the AES polynomial 0x11B (x^8+x^4+x^3+x+1)
/////////////////////////////////////////////

    // xtime (Ãƒâ€”2 in GF(2^8)) with reduction mod 0x11B
    function automatic [7:0] xtime(input [7:0] a);
        xtime = a[7] ? ({a[6:0],1'b0} ^ 8'h1b) : {a[6:0],1'b0};
    endfunction

    // Precomputed multiples from xtime chains 
    function automatic [7:0] mul2(input [7:0] a); // Computes multiplication by x
        mul2 = xtime(a);
    endfunction

    function automatic [7:0] mul4(input [7:0] a); // Computes multiplication by x^2
        mul4 = xtime(mul2(a));
    endfunction

    function automatic [7:0] mul8(input [7:0] a); // Computes multiplication by x^3
        mul8 = xtime(mul4(a));
    endfunction

    function automatic [7:0] mul9(input [7:0] a);
        mul9 = mul8(a) ^ a;             // 0x09
    endfunction

    function automatic [7:0] mulB(input [7:0] a);
        mulB = mul8(a) ^ mul2(a) ^ a;   // 0x0B
    endfunction

    function automatic [7:0] mulD(input [7:0] a);
        mulD = mul8(a) ^ mul4(a) ^ a;   // 0x0D
    endfunction

    function automatic [7:0] mulE(input [7:0] a);
        mulE = mul8(a) ^ mul4(a) ^ mul2(a); // 0x0E
    endfunction

/////////////////////////////////////////////                  
// inv_mixcolumn
//  performs matrix inverse multiplication on a single column 
//  follows inverse MixColumns logic defined in Section 5.3.3 EQ(5.15) of NIST: FIPS-197
/////////////////////////////////////////////

module inv_mixcolumn(input  logic [31:0] a,
                     output logic [31:0] y);

    logic [7:0] a0, a1, a2, a3, y0, y1, y2, y3;

    assign {a0,a1,a2,a3} = a;

    // Row 0: 0EÃ‚Â·a0 ^ 0BÃ‚Â·a1 ^ 0DÃ‚Â·a2 ^ 09Ã‚Â·a3
    assign y0 = mulE(a0) ^ mulB(a1) ^ mulD(a2) ^ mul9(a3);

    // Row 1: 09Ã‚Â·a0 ^ 0EÃ‚Â·a1 ^ 0BÃ‚Â·a2 ^ 0DÃ‚Â·a3
    assign y1 = mul9(a0) ^ mulE(a1) ^ mulB(a2) ^ mulD(a3);

    // Row 2: 0DÃ‚Â·a0 ^ 09Ã‚Â·a1 ^ 0EÃ‚Â·a2 ^ 0BÃ‚Â·a3
    assign y2 = mulD(a0) ^ mul9(a1) ^ mulE(a2) ^ mulB(a3);

    // Row 3: 0BÃ‚Â·a0 ^ 0DÃ‚Â·a1 ^ 09Ã‚Â·a2 ^ 0EÃ‚Â·a3
    assign y3 = mulB(a0) ^ mulD(a1) ^ mul9(a2) ^ mulE(a3);

    assign y = {y0, y1, y2, y3};
endmodule

/////////////////////////////////////////////                  
// inv_mixcolumns
//  Inverse MixColumns transformation
/////////////////////////////////////////////
module inv_mixcolumns(input  logic [127:0] a,
                      output logic [127:0] y);

    inv_mixcolumn imc0(a[127:96], y[127:96]);
    inv_mixcolumn imc1(a[95:64],  y[95:64]);
    inv_mixcolumn imc2(a[63:32],  y[63:32]);
    inv_mixcolumn imc3(a[31:0],   y[31:0]);
endmodule

/////////////////////////////////////////////
// inv_mixcolumns_sync
//   Pipelined version with 1 register stage
//   to break up the combinational path
/////////////////////////////////////////////
module inv_mixcolumns_sync(
    input  logic clk,
    input  logic [127:0] a,
    output logic [127:0] y
);
    logic [127:0] stage1;

    always_ff @(posedge clk) begin
        stage1 <= a;
    end
    
    // Computational stage
    inv_mixcolumns imc(stage1, y);
endmodule

/////////////////////////////////////////////                  
// inv_subBytes
//   inverse byte substitution, matches subBytes I/O & latency
/////////////////////////////////////////////

module inv_subBytes(input logic clk,
                    input  logic [127:0] a,
                    output logic [127:0] y);

    inv_sbox_sync isb0(a[127:120], clk, y[127:120]);
    inv_sbox_sync isb1(a[119:112], clk, y[119:112]);
    inv_sbox_sync isb2(a[111:104], clk, y[111:104]);
    inv_sbox_sync isb3(a[103:96] , clk, y[103:96]);
    inv_sbox_sync isb4(a[95:88]  , clk, y[95:88]);
    inv_sbox_sync isb5(a[87:80]  , clk, y[87:80]);
    inv_sbox_sync isb6(a[79:72]  , clk, y[79:72]);
    inv_sbox_sync isb7(a[71:64]  , clk, y[71:64]);
    inv_sbox_sync isb8(a[63:56]  , clk, y[63:56]);
    inv_sbox_sync isb9(a[55:48]  , clk, y[55:48]);
    inv_sbox_sync isb10(a[47:40] , clk, y[47:40]);
    inv_sbox_sync isb11(a[39:32] , clk, y[39:32]);
    inv_sbox_sync isb12(a[31:24] , clk, y[31:24]);
    inv_sbox_sync isb13(a[23:16] , clk, y[23:16]);
    inv_sbox_sync isb14(a[15:8]  , clk, y[15:8]);
    inv_sbox_sync isb15(a[7:0]   , clk, y[7:0]);

endmodule

/////////////////////////////////////////////                  
// inv_shiftRows
//   Right-rotate rows by 0/1/2/3 bytes
/////////////////////////////////////////////

module inv_shiftRows(input  logic [127:0] a,
                     output logic [127:0] y);
    // row 0 unchanged
    assign y[127:120] = a[127:120];
    assign y[95:88]   = a[95:88];
    assign y[63:56]   = a[63:56];
    assign y[31:24]   = a[31:24];

    // row 1 shifted right by 1
    assign y[119:112] = a[23:16];
    assign y[87:80]   = a[119:112];
    assign y[55:48]   = a[87:80];
    assign y[23:16]   = a[55:48];

    // row 2 shifted right by 2
    assign y[111:104] = a[47:40];
    assign y[79:72]   = a[15:8];
    assign y[47:40]   = a[111:104];
    assign y[15:8]    = a[79:72];

    // row 3 shifted right by 3
    assign y[103:96]  = a[71:64];
    assign y[71:64]   = a[39:32];
    assign y[39:32]   = a[7:0];
    assign y[7:0]     = a[103:96];
endmodule

/////////////////////////////////////////////
// addRoundKey
//   addRoundKey portion of AES algorithm
//   Section 5.1.4, Figure 5
/////////////////////////////////////////////

module addRoundKey(input  logic [127:0] a,
                   input  logic [127:0] k,
                   output logic [127:0] y);
                   
    assign y = a ^ k;
endmodule

/////////////////////////////////////////////
// getNextKeyEIC
//   Mimics getNextKey for AES encryption
//
//   Key expansion portion of AES algorithm
//   Takes previous key and rcon
//   rotates the previous key and applies sbox to each byte
//   XOR with rcon and the previous key to get the new key
//   similar to getNextKey for encryption
//   Section 5.2, Figure 6
/////////////////////////////////////////////

module getNextKeyEIC(input  logic clk,
                  input  logic [3:0][31:0] currKey,
                  input  logic [31:0] rcon,
                  output logic [3:0][31:0] nextKey);
                    
    logic [31:0] t;
    logic [7:0]  t0, t1, t2, t3, s0, s1, s2, s3;
    logic [7:0]  s0_reg, s1_reg, s2_reg, s3_reg;
    logic [3:0][31:0] currKey_reg;
    logic [31:0] rcon_reg;

    // Rotate left by 8 bits
    assign {t0, t1, t2, t3} = currKey[0];
    assign t = {t1, t2, t3, t0};

    // Apply synchronous sbox to each byte
    sbox_sync sb0(t[31:24], clk, s0);
    sbox_sync sb1(t[23:16], clk, s1);
    sbox_sync sb2(t[15:8],  clk, s2);
    sbox_sync sb3(t[7:0],   clk, s3);
    
    // Register the sbox outputs and inputs for next stage
    always_ff @(posedge clk) begin
        s0_reg <= s0;
        s1_reg <= s1;
        s2_reg <= s2;
        s3_reg <= s3;
        currKey_reg <= currKey;
        rcon_reg <= rcon;
    end
    
    // Generate next key from registered values
    assign nextKey[3] = currKey_reg[3] ^ ({s0_reg, s1_reg, s2_reg, s3_reg} ^ rcon_reg);
    assign nextKey[2] = currKey_reg[2] ^ nextKey[3];
    assign nextKey[1] = currKey_reg[1] ^ nextKey[2];
    assign nextKey[0] = currKey_reg[0] ^ nextKey[1];
endmodule