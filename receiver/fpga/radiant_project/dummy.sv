module aes_spi(input  logic sck, 
               input  logic sdi,
               output logic sdo,
               input  logic done,
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
        wasdone = done;
        sdodelayed = plaintextcaptured[126];
    end
    
    // when done is first asserted, shift out msb before clock edge
    assign sdo = (done & !wasdone) ? plaintext[127] : sdodelayed;
endmodule