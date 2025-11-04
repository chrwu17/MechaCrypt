// Josaphat Ngoga
// jngoga@g.hmc.edu
// 10/15/2025

/////////////////////////////////////////////
// aes
//   Top level module with SPI interface and SPI core
/////////////////////////////////////////////

module aes(input  logic clk,
           input  logic sck, 
           input  logic sdi,
           output logic sdo,
           input  logic load,
           output logic done);
                    
    logic [127:0] key, plaintext, cyphertext;
            
    aes_spi spi(sck, sdi, sdo, done, key, plaintext, cyphertext);   
    aes_decrypt_core core(clk, load, key, plaintext, done, cyphertext);
endmodule

/////////////////////////////////////////////
// aes_spi
//   SPI interface.  Shifts in key and plaintext
//   Captures ciphertext when done, then shifts it out
//   Tricky cases to properly change sdo on negedge clk
/////////////////////////////////////////////

module aes_spi(input  logic sck, 
               input  logic sdi,
               output logic sdo,
               input  logic done,
               output logic [127:0] key, plaintext,
               input  logic [127:0] cyphertext);

    logic         sdodelayed, wasdone;
    logic [127:0] cyphertextcaptured;
               
    // assert load
    // apply 256 sclks to shift in key and plaintext, starting with plaintext[127]
    // then deassert load, wait until done
    // then apply 128 sclks to shift out cyphertext, starting with cyphertext[127]
    // SPI mode is equivalent to cpol = 0, cpha = 0 since data is sampled on first edge and the first
    // edge is a rising edge (clock going from low in the idle state to high).
    always_ff @(posedge sck)
        if (!wasdone)  {cyphertextcaptured, plaintext, key} = {cyphertext, plaintext[126:0], key, sdi};
        else           {cyphertextcaptured, plaintext, key} = {cyphertextcaptured[126:0], plaintext, key, sdi}; 
    
    // sdo should change on the negative edge of sck
    always_ff @(negedge sck) begin
        wasdone = done;
        sdodelayed = cyphertextcaptured[126];
    end
    
    // when done is first asserted, shift out msb before clock edge
    assign sdo = (done & !wasdone) ? cyphertext[127] : sdodelayed;
endmodule


/////////////////////////////////////////////
// aes_decrypt_core
//   AES decryption core
//   Follows SP Network structure similar to encryption
//
//   Decryption follows Equivalent Inverse Cipher algorithm, mimicing encryption datapath
//   for easier hardware reuse and debuggind
//   
//   Round keys are generated at the start of decryption and stored since deccryption starts with K10
//   and works backwards to K0. 
//   Key schedule is run for all 10 rounds to generate K1..K10, and K0 is the original key.
//   EqInv requires pre-mixing round keys 1..9 by applying InvMixColumns while keeping K0 and K10 unchanged.
//   This is also done during key schedule generation to save time during decryption rounds.
/////////////////////////////////////////////

module aes_decrypt_core(
    input  logic         clk, 
    input  logic         decrypt,           // NEW
    input  logic [127:0] key, 
    input  logic [127:0] cyphertext, 
    output logic         done_decrypt,    // NEW
    output logic [127:0] plaintext);

    // Internal signals
    logic ka_busy, ka_done; // advance key schedule signals
    logic [3:0][31:0] currKey, nextKey, word;
    logic [31:0] rcon;
    logic [3:0] ka_round, round_idx, roundCount, cycleCount;
    logic [127:0] state; // Holds intermediate state of the data
    logic [127:0] bfrSub, afterSub, afterShift, afterMix, bfrAdd, afterAdd, keyPreMix, keyAfterMix;

    // Inverse Data path signals and setup
    inv_subBytes sub(clk, bfrSub, afterSub);
    inv_shiftRows shift(afterSub, afterShift);
    inv_mixcolumns mix(afterShift, afterMix);
    addRoundKey add(bfrAdd, {word[3], word[2], word[1], word[0]}, afterAdd);

    getNextKeyEIC keyExp(clk, currKey, rcon, nextKey);

    // premix keys
    assign keyPreMix = {nextKey[3], nextKey[2], nextKey[1], nextKey[0]}; // pack 4x32-bit words into 128-bit for mixing
    inv_mixcolumns premixKeys(keyPreMix, keyAfterMix);

    // Arrays to store original and premixed keys for rounds
    logic [10:0][127:0] roundKeys, premixedKeys;

    always_ff @(posedge clk) begin
        if (decrypt) begin
            roundCount <= 10;
            cycleCount <= 0;
            done_decrypt <= 0;

            // If begining, load key and cyphertext
            roundKeys[0] <= {key[127:96], key[95:64], key[63:32], key[31:0]}; // Key for round 0
            currKey <= {key[127:96], key[95:64], key[63:32], key[31:0]};

            // Initial input cyphertext
            bfrAdd <= cyphertext;

            // start key schedule advance to build K1..K10 (+ premix K1..K9)
            ka_round <= 0;
            ka_busy <= 1;
            ka_done <= 0;

        end else if (ka_busy) begin // Key schedule advance
            if (cycleCount == 0) begin
                currKey <= nextKey; // step the schedule

                // Store the raw round keys
                roundKeys[ka_round + 1] <= {nextKey[3], nextKey[2], nextKey[1], nextKey[0]};

                // Store the premixed round keys for rounds 1..9
                if ((ka_round + 1) >= 1 && (ka_round + 1) <= 9) begin
                    premixedKeys[ka_round + 1] <= keyAfterMix; // InvMixColumns applied
                end else begin
                    premixedKeys[ka_round + 1] <= {nextKey[3], nextKey[2], nextKey[1], nextKey[0]}; // No InvMixColumns for K0 and K10
                end
            end

            // After advancing till K10
            if ((ka_round == 9) && (cycleCount == 0)) begin
                ka_busy <= 0;
                ka_done <= 1;
            end else if (cycleCount == 0) begin
                ka_round <= ka_round + 1;
            end

        end else if (!done_decrypt & ka_done) begin
            if (roundCount == 10) begin
                if (cycleCount == 0) begin
                    // Use raw K10 for initial pass
                    word <= {roundKeys[10][127:96], roundKeys[10][95:64], roundKeys[10][63:32], roundKeys[10][31:0]};
                end if (cycleCount == 3) begin
                    state <= afterAdd; // First state
                end
            end

            // Process rounds (9 - 1) with premixed keys
            if ((roundCount > 0) && (roundCount < 10)) begin
                if (cycleCount == 0) begin
                    // Use premixed keys for rounds 1..9
                    word <= {premixedKeys[roundCount][127:96], premixedKeys[roundCount][95:64], premixedKeys[roundCount][63:32], premixedKeys[roundCount][31:0]};
                end if (cycleCount == 1) begin // one-cycle delay for sbox
                    bfrSub <= state;
                end if (cycleCount == 2) begin
                    bfrAdd <= afterMix;
                end if (cycleCount == 3) begin
                    state <= afterAdd; // Next state
                end
            end 

            // If it's round 0, we're done. Skip column mixing.
            if (roundCount == 0) begin
                if (cycleCount == 0) begin
                    // Use raw K0 for final round
                    word <= {roundKeys[0][127:96], roundKeys[0][95:64], roundKeys[0][63:32], roundKeys[0][31:0]};
                end if (cycleCount == 1) begin
                    bfrSub <= state;
                end if (cycleCount == 2) begin
                    bfrAdd <= afterShift; // Skip mixcolumns
                end if (cycleCount == 3) begin
                    plaintext <= afterAdd;
                    done_decrypt <= 1;
                end
            end

            // Update cycle and round counters
            if (cycleCount == 3) begin
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



/////////////////// AES Decryption Primitives for Equivalent Inverse Cipher ////////////////////////

/////////////////////////////////////////////
// sbox
//   Infamous AES byte substitutions with magic numbers
//   Synchronous version which is mapped to embedded block RAMs (EBR)
//   Section 5.1.1, Figure 7
/////////////////////////////////////////////
module sbox_sync(input		logic [7:0] a,
                 input	 	logic clk,
                 output 	logic [7:0] y);
            
    // sbox implemented as a ROM
    // This module is synchronous and will be inferred using BRAMs (Block RAMs)
    logic [7:0] sbox [0:255];

    initial   $readmemh("sbox.txt", sbox);
    
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
    
    initial $readmemh("inv_sbox.txt", invsbox);
    
    always_ff @(posedge clk) begin
        y <= invsbox[a];
    end
endmodule

/////////////////////////////////////////////                  
// inv_mixcolumns
//  Inverse MixColumns transformation
/////////////////////////////////////////////
module inv_mixcolumns(input  logic [127:0] a,
                      output logic [127:0] y);

    inv_mixcolumn c0(a[127:96], y[127:96]);
    inv_mixcolumn c1(a[95:64],  y[95:64]);
    inv_mixcolumn c2(a[63:32],  y[63:32]);
    inv_mixcolumn c3(a[31:0],   y[31:0]);
endmodule

/////////////////////////////////////////////                  
// inv_mixcolumn
//  performs matrix inverse multiplication on a single column 
//  follows inverse MixColumns logic defined in Section 5.3.3 EQ(5.15) of NIST: FIPS-197
/////////////////////////////////////////////

module inv_mixcolumn(input  logic [31:0] a,
                     output logic [31:0] y);

    logic [7:0] a0, a1, a2, a3, y0, y1, y2, y3;

    assign {a0,a1,a2,a3} = a;

    // Row 0: 0E·a0 ^ 0B·a1 ^ 0D·a2 ^ 09·a3
    assign y0 = mulE(a0) ^ mulB(a1) ^ mulD(a2) ^ mul9(a3);

    // Row 1: 09·a0 ^ 0E·a1 ^ 0B·a2 ^ 0D·a3
    assign y1 = mul9(a0) ^ mulE(a1) ^ mulB(a2) ^ mulD(a3);

    // Row 2: 0D·a0 ^ 09·a1 ^ 0E·a2 ^ 0B·a3
    assign y2 = mulD(a0) ^ mul9(a1) ^ mulE(a2) ^ mulB(a3);

    // Row 3: 0B·a0 ^ 0D·a1 ^ 09·a2 ^ 0E·a3
    assign y3 = mulB(a0) ^ mulD(a1) ^ mul9(a2) ^ mulE(a3);

    assign y = {y0, y1, y2, y3};
endmodule

/////////////////////////////////////////////                  
// galois_mult_ext
//   Provides GF(2^8) multiplication helpers for AES inverse MixColumns coefficients
//   performs polynomial multiplication by x^i for i in {1,2,3} via xtime
//   then combines to get {09, 0B, 0D, 0E} multiplications as shown below:
//        0x09 = x^3 + 1
//        0x0B = x^3 + x + 1
//        0x0D = x^3 + x^2 + 1
//        0x0E = x^3 + x^2 + x
//   and reduction mod the AES polynomial 0x11B (x^8+x^4+x^3+x+1)
/////////////////////////////////////////////

module galois_mult_ext;

    // xtime (×2 in GF(2^8)) with reduction mod 0x11B
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

endmodule

/////////////////////////////////////////////                  
// inv_subBytes
//   inverse byte substitution, matches subBytes I/O & latency
/////////////////////////////////////////////

module inv_subBytes(input logic clk,
                    input  logic [127:0] a,
                    output logic [127:0] y);

    inv_sbox_sync sb0(a[127:120], clk, y[127:120]);
    inv_sbox_sync sb1(a[119:112], clk, y[119:112]);
    inv_sbox_sync sb2(a[111:104], clk, y[111:104]);
    inv_sbox_sync sb3(a[103:96] , clk, y[103:96]);
    inv_sbox_sync sb4(a[95:88]  , clk, y[95:88]);
    inv_sbox_sync sb5(a[87:80]  , clk, y[87:80]);
    inv_sbox_sync sb6(a[79:72]  , clk, y[79:72]);
    inv_sbox_sync sb7(a[71:64]  , clk, y[71:64]);
    inv_sbox_sync sb8(a[63:56]  , clk, y[63:56]);
    inv_sbox_sync sb9(a[55:48]  , clk, y[55:48]);
    inv_sbox_sync sb10(a[47:40] , clk, y[47:40]);
    inv_sbox_sync sb11(a[39:32] , clk, y[39:32]);
    inv_sbox_sync sb12(a[31:24] , clk, y[31:24]);
    inv_sbox_sync sb13(a[23:16] , clk, y[23:16]);
    inv_sbox_sync sb14(a[15:8]  , clk, y[15:8]);
    inv_sbox_sync sb15(a[7:0]   , clk, y[7:0]);

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

    // row 1 right by 1
    assign y[119:112] = a[23:16];
    assign y[87:80]   = a[119:112];
    assign y[55:48]   = a[87:80];
    assign y[23:16]   = a[55:48];

    // row 2 right by 2
    assign y[111:104] = a[47:40];
    assign y[79:72]   = a[15:8];
    assign y[47:40]   = a[111:104];
    assign y[15:8]    = a[79:72];

    // row 3 right by 3
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
    
    // rotate left by 8 bits
    assign {t0, t1, t2, t3} = currKey[0];
    assign t = {t1, t2, t3, t0};
    
    // apply sbox to each byte of t
    sbox_sync sb0(t[31:24], clk, s0);
    sbox_sync sb1(t[23:16], clk, s1);
    sbox_sync sb2(t[15:8], clk, s2);
    sbox_sync sb3(t[7:0], clk, s3);
    
    // generate next words
    assign nextKey[0] = currKey[0] ^ ({s0, s1, s2, s3} ^ rcon);
    assign nextKey[1] = currKey[1] ^ nextKey[0];
    assign nextKey[2] = currKey[2] ^ nextKey[1];
    assign nextKey[3] = currKey[3] ^ nextKey[2];
endmodule
