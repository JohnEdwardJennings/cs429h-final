`timescale 1ps/1ps

module branchPredictor(input clk, input isValidBranchIns, input isJumping, input pcToJumpFromWasMisaligned,
        input [15:0] pc, input [15:0] pcWB, input [15:0] writeData, 
        output predictJump, output [15:0] predictedTarget);

     // direct mapping cache implementation (NUM_ENTRIES rows with each one containing a register for -> tag, value, valid bits, and multiple registers for 2-bit history and 2-level predictor)

    wire predictJump; // returns true if the branch predictor predicted a jump for particular instruction
    wire [15:0] predictedTarget;
    parameter NUM_BITS = 10;
    parameter NUM_ENTRIES = 1 << NUM_BITS; // entry size = 2^NUM_BITS

    wire [1:0] saturatedCounterToUse; // used to determine which of the 4 states to use based on previous 2 results of given PC with branch instruction (used in fetch 0)
    reg [15 - NUM_BITS:0] BPTags[0:NUM_ENTRIES]; // stores the PCs with branch instructions
    reg [15:0] BPTargets[0:NUM_ENTRIES]; // stores the corresponding target PCs
    reg [3:0] BPBoolFlags[0:NUM_ENTRIES]; // index 0..3 = if particular pair of bits is valid or not (ie if history has already been stored or not)

    reg [1:0] prevTwoJumps[0:NUM_ENTRIES]; // used to track history of each branch result (0 = not taken, 1 = taken)

    // each saturated counter corresponds to one of the four 2-bit history pairs
    reg [1:0] saturatedCounter00[0:NUM_ENTRIES];
    reg [1:0] saturatedCounter01[0:NUM_ENTRIES];
    reg [1:0] saturatedCounter10[0:NUM_ENTRIES];
    reg [1:0] saturatedCounter11[0:NUM_ENTRIES];

    reg [NUM_BITS:0] i;

    initial begin
        for (i = 0; i < NUM_ENTRIES; i = i + 1) begin
            BPBoolFlags[i] = 5'b0;
            BPTags[i] = 6'b1; // so that there is no PC equal to it initially
            prevTwoJumps[i] = 2'b00;
            saturatedCounter00[i] = 2'b01;
            saturatedCounter01[i] = 2'b01;
            saturatedCounter10[i] = 2'b01;
            saturatedCounter11[i] = 2'b01;
        end
    end

    wire [NUM_BITS - 1:0] hashVal = pc[NUM_BITS - 1:0]; // mod by NUM_ENTRIES
    wire [NUM_BITS - 1:0] hashValWB = pcWB[NUM_BITS - 1:0];

    assign saturatedCounterToUse =  (prevTwoJumps[hashVal] == 0) ? saturatedCounter00[hashVal] :
                                    (prevTwoJumps[hashVal] == 1) ? saturatedCounter01[hashVal] :
                                    (prevTwoJumps[hashVal] == 2) ? saturatedCounter10[hashVal] :
                                    saturatedCounter11[hashVal];

    assign predictedTarget = BPTargets[hashVal]; // will only be relevant if predictJump is 1
    assign predictJump = (!pcToJumpFromWasMisaligned && ~(pc[0] && !predictedTarget[0])
                        && BPTags[hashVal] == pc[15:NUM_BITS] 
                        && BPBoolFlags[hashVal][prevTwoJumps[hashVal]] 
                         && saturatedCounterToUse[1]); // predictJump is set to true based on saturating counters
    always @(posedge clk) begin
        // logic to update branch predictor data structures (ie history and hash table)
        if (isValidBranchIns) begin
            prevTwoJumps[hashValWB][0] <= prevTwoJumps[hashValWB][1];
            prevTwoJumps[hashValWB][1] <= isJumping; // updates last 2 branch results for the given PC entry

            if (!BPBoolFlags[hashValWB][prevTwoJumps[hashValWB]]) begin // checks if the pair of 2 bits have a valid prediction
                BPBoolFlags[hashValWB][prevTwoJumps[hashValWB]] <= 1; 
            end

            BPTags[hashValWB] <= pcWB[15:NUM_BITS];
            BPTargets[hashValWB] <= writeData;

            // updates saturated counters based on isJumping and the branch's last 2 branching results 
            if (isJumping) begin
                if (prevTwoJumps[hashValWB] == 0) begin 
                    saturatedCounter00[hashValWB] <= (saturatedCounter00[hashValWB] != 3) ? 
                            saturatedCounter00[hashValWB] + 1 : saturatedCounter00[hashValWB];
                end
                else if (prevTwoJumps[hashValWB] == 1) begin 
                    saturatedCounter01[hashValWB] <= (saturatedCounter01[hashValWB] != 3) ? 
                            saturatedCounter01[hashValWB] + 1 : saturatedCounter01[hashValWB];
                end
                else if (prevTwoJumps[hashValWB] == 2) begin 
                    saturatedCounter10[hashValWB] <= (saturatedCounter10[hashValWB] != 3) ? 
                            saturatedCounter10[hashValWB] + 1 : saturatedCounter10[hashValWB];
                end
                else if (prevTwoJumps[hashValWB] == 3) begin 
                    saturatedCounter11[hashValWB] <= (saturatedCounter11[hashValWB] != 3) ? 
                            saturatedCounter11[hashValWB] + 1 : saturatedCounter11[hashValWB];
                end
            end else begin
                if (prevTwoJumps[hashValWB] == 0) begin 
                    saturatedCounter00[hashValWB] <= (saturatedCounter00[hashValWB] != 0) ? 
                            saturatedCounter00[hashValWB] - 1 : saturatedCounter00[hashValWB];
                end
                else if (prevTwoJumps[hashValWB] == 1) begin 
                    saturatedCounter01[hashValWB] <= (saturatedCounter01[hashValWB] != 0) ? 
                            saturatedCounter01[hashValWB] - 1 : saturatedCounter01[hashValWB];
                end
                else if (prevTwoJumps[hashValWB] == 2) begin 
                    saturatedCounter10[hashValWB] <= (saturatedCounter10[hashValWB] != 0) ? 
                            saturatedCounter10[hashValWB] - 1 : saturatedCounter10[hashValWB];
                end
                else if (prevTwoJumps[hashValWB] == 3) begin 
                    saturatedCounter11[hashValWB] <= (saturatedCounter11[hashValWB] != 0) ? 
                            saturatedCounter11[hashValWB] - 1 : saturatedCounter11[hashValWB];
                end
            end
        end
    end

endmodule
