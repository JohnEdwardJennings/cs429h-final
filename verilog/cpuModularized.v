`timescale 1ps/1ps
module main();

    initial begin
        $dumpfile("cpu.vcd");
        $dumpvars(0,main);
    end

    // clock
    wire clk;
    clock c0(clk);

    reg halt = 0;

    counter ctr(halt,clk);

    // valid bits (all of my stages are 0-indexed)
    reg fetch0_v;
    reg fetch1_v;
    reg decode_v;
    reg exec0_v;
    reg exec1_v;
    reg wb_v;

    wire isJumping; // declared early for flushing (though its only used in write back)
    wire incorrectPrediction; // if set to 1, then the whole pipeline is flushed
    wire isStallingF0; // if any of these signals are 1, then all the stall signals to the left should also be 1
    wire isStallingF1; 
    wire isStallingDecode; 
    wire misalignedLoadDetected; 
    wire isSelfModifying; // another flushing pipeline condition
    wire [2:0] selfModifyingStage; // specifies which stage is being self-modified (to enable partial flushing)
    wire isOdd; // to determine if PC is odd for misaligned PCs

    reg [1:0] F1GotStalled; // to ensure fetch 1's instruction doesn't go to decode when fetch_1 is stalled
    reg useDecodeInsAgain; // set true when decode is stalled so it reuses decode instruction
    reg useF1InsAgain; // set true when decode becomes unstalled so that it uses the stalled F1 instruction
    reg [15:0] misalignedIns;
    reg isMisaligned; // set true when PC is odd, indicating instruction must concatenate with misalignedIns

    reg [15:0] stalled_vA; // used for exec0 stage for misaligned loads/stores
    reg [15:0] stalled_vB; // used for exec0 stage for misaligned loads/stores
    reg firstLoadOccured; // used for misaligned stores and loads to allow for "2 load instructions in exec_0"
    reg loadMisalignmentOccured; // used to concatenate the two load addresses in write_back stage
    reg usingSecondLoad; // for writing/reading from misaligned memory addresses

    // all store the outputs from the branch predictor
    wire predictJump;
    wire [15:0] predictedTarget;
    wire isValidBranchIns;
    wire [15:0] writeData; // result from the write_back stage that will be written into destination register

    // to store if current instruction predicted a jump or not
    reg predictJumpF1;
    reg predictJumpDC;
    reg predictJumpE0;
    reg predictJumpE1;
    reg predictJumpWB;

    reg [15:0] predictedTargetF1;
    reg [15:0] predictedTargetDC;
    reg [15:0] predictedTargetE0;
    reg [15:0] predictedTargetE1;
    reg [15:0] predictedTargetWB;

    // these are used to deal with jumping to and from misaligned PCs for the branch predictor
    reg targetPredictionWasMisaligned;
    reg [15:0] predictedTargetFromMisalignedPC;
    reg pcToJumpFromWasMisaligned;
    reg stallOneCycleForMisalignedPrediction;
    reg predictedMisalignedPCF1; // used to stall for 1 cycle when predicting jumps from misaligned PCs
    initial begin
        fetch0_v = 1;
        fetch1_v = 0;
        decode_v = 0;
        exec0_v = 0;
        exec1_v = 0;
        wb_v = 0;
        F1GotStalled = 0;
        useDecodeInsAgain = 0;
        useF1InsAgain = 0;
        isMisaligned = 0;
        firstLoadOccured = 0;
        loadMisalignmentOccured = 0;
        usingSecondLoad = 0;
        predictedMisalignedPCF1 = 0;
        targetPredictionWasMisaligned = 0;
        pcToJumpFromWasMisaligned = 0;
    end

    // updating valid states based on clock ticks and logic involving the flush bits and stall bits
    always @(posedge clk) begin
        fetch1_v <= fetch0_v & ~incorrectPrediction && ~(isSelfModifying && selfModifyingStage >= 1);
        decode_v <= fetch1_v & ~incorrectPrediction && ~(isSelfModifying && selfModifyingStage >= 2) 
                    & (((isMisaligned || ~isOdd) && ~stallOneCycleForMisalignedPrediction) || isStallingDecode); 
        exec0_v <= decode_v & ~incorrectPrediction && ~(isSelfModifying && selfModifyingStage >= 3) &
                   (~isStallingDecode | (misalignedLoadDetected & ~firstLoadOccured));
        exec1_v <= exec0_v & ~incorrectPrediction && ~(isSelfModifying && selfModifyingStage >= 4);
        wb_v <= exec1_v & ~incorrectPrediction && ~(isSelfModifying && selfModifyingStage >= 5);
    end

    // PC
    reg [15:0] pc = 16'h0000;
    reg [15:0] pcF1; // pc that is retrieved in fetch 1 from fetch 0
    reg [15:0] pcDC; // pc for decode instruction
    reg [15:0] pcE0; // pc for execute_0 instruction
    reg [15:0] pcE1; // pc for exec_1 instruction
    reg [15:0] pcWB; // pc for write back instruction

    /**********
     * Fetch (Part 0) *
     **********/

    // predictJump and predictedTarget are the output wires
    branchPredictor branchPredictor(clk, isValidBranchIns, isJumping, pcToJumpFromWasMisaligned, 
            pc, pcWB, writeData, predictJump, predictedTarget);
    
    // read from memory
    wire [15:0] loadAddr; // address used for load instruction
    wire [15:0] instr; // instruction that pc in fetch points to
    wire [15:0] loadedData; // data returned from memory in load instruction
    wire [15:0] storeAddress;
    wire [15:0] storedData;
    wire strEN; // write enable bit specifically for store instruction

    mem mem(clk,
          pc[15:1],instr,
          loadAddr[15:1],loadedData,strEN,storeAddress[15:1], storedData);

    assign isStallingF0 = isStallingF1;
    reg tempPredJump;
    reg [15:0] tempPredJumpTarget;
    always @(posedge clk) begin
        if (!isStallingF1) begin
            pcF1 <= pc;
            tempPredJump <= (predictJump) ? predictJump : 0;
            predictedTargetFromMisalignedPC <= (predictedTarget) ? predictedTarget : 0;

            targetPredictionWasMisaligned <= (predictJump && predictedTarget[0]) ? 1 : 0;
            pcToJumpFromWasMisaligned <= (predictJump && pc[0]) ? 1 : 0;
            predictedMisalignedPCF1 <= pcToJumpFromWasMisaligned;
            if ((predictJump && pc[0]) || pcToJumpFromWasMisaligned) begin // if jumping from misaligned pc, need to make sure next word is read before prediction occurs
                predictJumpF1 <= tempPredJump;
                predictedTargetF1 <= predictedTargetFromMisalignedPC;
            end else begin
                predictJumpF1 <= predictJump;
                predictedTargetF1 <= predictedTarget;
            end
        end
    end 

    /**********
     * Fetch (Part 1) *
     **********/

    // dealing with misaligned PC
    assign isOdd = (fetch1_v && pcF1[0] == 1);
    wire [15:0] ins = (isOdd && isMisaligned) ? {instr[7:0], misalignedIns[15:8]} : instr;
    reg [15:0] pcForDecodeIfMisaligned; // will be set to pcDC if the fetch PC is misaligned
    reg predictJumpDCForMisaligned;
    reg [15:0] predictedTargetDCForMisaligned;

    reg [15:0] insF1Stalled; // instruction returned by f1 stage in memory when f1 is stalled
    assign isStallingF1 = isStallingDecode;

    reg [15:0] insF1StalledMisaligned; // instruction returned by f1 stage in memory when f1 is stalled
    reg F1WasStalledForTwoCycles;
    reg doNotUpdateMisalignedIns;
    always @(posedge clk) begin
        if (!isStallingDecode) begin // if decode is stalled, so must Fetch_1
            if (F1GotStalled) begin
                useF1InsAgain <= 1; // register flag to ensure decode uses the stalled f1 instruction instead of new f1 instruction (which is anyways being re-read in f0)
                insF1Stalled <= (F1GotStalled == 2) ? insF1Stalled : 
                                (F1GotStalled == 1 && (isMisaligned && isOdd) && F1WasStalledForTwoCycles) ?
                                insF1StalledMisaligned : ins;
                insF1StalledMisaligned <= ins;
                if (F1GotStalled == 2) begin 
                    F1WasStalledForTwoCycles <= 1;
                end
                if (F1GotStalled == 1 && !F1WasStalledForTwoCycles) begin
                    doNotUpdateMisalignedIns <= 1; // to prevent a misaligned instruction from being skipped when one stall occurs
                end
                F1GotStalled <= (isMisaligned && isOdd) ? F1GotStalled - 1 : 0;
            end else begin
                useF1InsAgain <= 0; // so that f1's stored instruction will only be used in decode the cycle right after decode is unstalled
                F1WasStalledForTwoCycles <= 0;
            end

            if (F1GotStalled < 2) begin 
                if (doNotUpdateMisalignedIns) begin
                    doNotUpdateMisalignedIns <= 0;
                end else begin
                    misalignedIns <= instr;
                end
            end 
            
            isMisaligned <= isOdd & ~incorrectPrediction & ~(isSelfModifying && selfModifyingStage >= 2);
            pcForDecodeIfMisaligned <= pcF1;
            if (fetch1_v && (isMisaligned && isOdd) && (pcForDecodeIfMisaligned + 2 < pcF1 
                    || pcForDecodeIfMisaligned - 2 > pcF1)) begin  // stall if predicted PC was misaligned
                stallOneCycleForMisalignedPrediction <= 1;
            end else if (fetch1_v && predictedMisalignedPCF1) begin  // stall if jumped from misaligned PC
                stallOneCycleForMisalignedPrediction <= 1;
            end else begin
                stallOneCycleForMisalignedPrediction <= 0;
            end

            pcDC <= (isOdd && isMisaligned) ? pcForDecodeIfMisaligned : pcF1;
            predictJumpDC <= predictJumpF1;
            predictedTargetDC <= predictedTargetF1;
        end else begin
            if (F1GotStalled == 1) begin // to store F1's instruction for 2 cycle stalls
                insF1Stalled <= ins; // stores instruction right before decode one
                misalignedIns <= instr;
            end
            if (F1GotStalled == 0) begin
                if (doNotUpdateMisalignedIns) begin
                    doNotUpdateMisalignedIns <= 0;
                end else begin
                    misalignedIns <= instr;
                end
            end
            F1GotStalled <= F1GotStalled + 1;
        end
    end

    /**********
     * Decode *
     **********/
    wire [15:0] insForDecode; // instruction used for decode stage
    reg [15:0] stalledIns; // stalled decode instruction

    assign insForDecode = (useDecodeInsAgain) ? stalledIns : 
                          (useF1InsAgain) ? insF1Stalled : // cycle right after decode is unstalled, it will use the stalled f1 instruction
                          ins;

    wire [3:0] opcode = insForDecode[15:12];
    wire [3:0] opcode2 = insForDecode[7:4]; // for jumping and load/store
    wire [3:0] rA = insForDecode[11:8]; // register A
    wire [3:0] rB = insForDecode[7:4]; // register B (either is 2nd read register or nothing, depending on ins)
    wire [3:0] rT = insForDecode[3:0]; // destination register (or can be 2nd read register for mov/jump/store instructions)
    wire [7:0] imm8 = insForDecode[11:4]; // for the mov instructions

    wire [15:0] valueA; // value stored in A (will be used in exec_0)
    wire [15:0] valueB; // value stored in B (will be used in exec_0)
    wire [15:0] result; // result of the ALU operations (for execute stages)

    reg [15:0] writeDataReg; // register that latches writeData from exec_1 into write_back stage
    reg [3:0] destRegWB; // needed for writing to correct rT and for forwarding wb to e0 and wb to e1
    wire wEN; // write enabling wire

    // decoding opcode
    wire [8:0] insType; // holds all the "control" signals
    assign insType = (opcode == 4'b0000) ? 9'b000000001 :  // sub
                     (opcode == 4'b1000) ? 9'b000000010 :  // movl
                     (opcode == 4'b1001) ? 9'b000000100 :  // movh
                     (opcode == 4'b1110 && opcode2 == 4'b0000) ? 9'b000001000 : // jz
                     (opcode == 4'b1110 && opcode2 == 4'b0001) ? 9'b000010000 : // jnz
                     (opcode == 4'b1110 && opcode2 == 4'b0010) ? 9'b000100000 : // js
                     (opcode == 4'b1110 && opcode2 == 4'b0011) ? 9'b001000000 : // jns
                     (opcode == 4'b1111 && opcode2 == 4'b0000) ? 9'b010000000 : // ldr
                     (opcode == 4'b1111 && opcode2 == 4'b0001) ? 9'b100000000 : // str
                     9'b0;                         // Undefined operation

    // logic to determine what is the 2nd read regsiter (it is T if movh or branch or store...else B)
    wire [3:0] secondReadReg = (insType[2] | insType[3] | insType[4] | insType[5] | insType[6] | insType[8]) ? rT
                                : rB;

    // registers
    regs regs(clk,
        rA,valueA,
        secondReadReg,valueB,
        wEN,destRegWB, writeData);
    
    // flip flops to preserve important values for the execute stages (these registers will be used in exec_0)
    reg [7:0] immE0;
    reg [8:0] insTypeE0;
    reg [3:0] firstSrcRegE0;
    reg [3:0] secondSrcRegE0;
    reg [3:0] destRegE0; // stores current rT for E0 (once rT changes to the next instruction's rT)

    // stall decode if load load hazard or misaligned load/store can occur
    assign isStallingDecode = (decode_v && exec0_v) && (((insTypeE0[7] == 1) && (insType[7] || insType[8]) 
            && (rA == destRegE0)) || (misalignedLoadDetected) || (insTypeE0[7] && valueA[0] && insType[8]));

    always @(posedge clk) begin
        if (isStallingDecode) begin
            useDecodeInsAgain <= 1;
            stalledIns <= insForDecode; // keeps decode instruction in "decode"
        end else begin
            useDecodeInsAgain <= 0; // so that decode instruction isn't repeated when no stall
            pcE0 <= pcDC;
            immE0 <= imm8;
            insTypeE0 <= insType;
            firstSrcRegE0 <= rA;
            secondSrcRegE0 <= secondReadReg;
            destRegE0 <= rT;
            predictJumpE0 <= predictJumpDC;
            predictedTargetE0 <= predictedTargetDC;
        end
    end

     /**********
     * Execute (Part 0) *
     **********/
    
    // all of these will be used in exec_1 stage
    reg [7:0] immediate;
    reg [8:0] instruction;
    reg [15:0] valueAE1; 
    reg [15:0] valueBE1;

    // this will be used in write_back stage
    reg [8:0] instructionForWB;

    wire [15:0] valueAIncluding0 = (firstSrcRegE0 == 0) ? 16'b0 : valueA; // checks for source registers == r0
    wire [15:0] valueBIncluding0 = (secondSrcRegE0 == 0) ? 16'b0 : valueB;

    // forwarding logic (exec_1 -> exec_0 and write_back -> exec_0)
    wire canForwardE1 = (exec1_v && (instruction[0] || instruction[1] || instruction[2] || instruction[7]));
    wire canForwardWB = (wb_v && (instructionForWB[0] || instructionForWB[1] || instructionForWB[2] || instructionForWB[7]));

    wire forwardRegA = canForwardE1 && (destRegE1 != 0) && (destRegE1 == firstSrcRegE0); // should only be true if exec_1 is in progress
    wire forwardRegB = canForwardE1 && (destRegE1 != 0) && (destRegE1 == secondSrcRegE0);

    wire forwardRegA_WB = canForwardWB && (destRegWB != 0) && (destRegWB == firstSrcRegE0); // should only be true if wb in progress
    wire forwardRegB_WB = canForwardWB && (destRegWB != 0) && (destRegWB == secondSrcRegE0);

    wire [15:0] vAForwarded =  (forwardRegA) ? result : 
                        (forwardRegA_WB) ? writeData : 
                        valueAIncluding0;
    wire [15:0] vBForwarded =  (forwardRegB) ? result : 
                        (forwardRegB_WB) ? writeData : 
                        valueBIncluding0;

    // load implementation (also checks for misaligned loads and stores)
    wire loadAddrIsOdd = (exec0_v && ((insTypeE0[7] || insTypeE0[8]) && vAForwarded[0]));

   assign misalignedLoadDetected = (loadAddrIsOdd) && ~firstLoadOccured; // stalls decode for 1 cycle if misaligned load or store scenario is detected

    assign loadAddr = (firstLoadOccured) ? stalled_vA + 2 : vAForwarded; // sends the correct value to memory as the "load address"

    // exec_1 versions of the three registers (necessary for forwarding)
    reg [3:0] firstSrcRegE1;
    reg [3:0] secondSrcRegE1;
    reg [3:0] destRegE1;
    always @(posedge clk) begin
        pcE1 <= pcE0;
        predictJumpE1 <= predictJumpE0;
        predictedTargetE1 <= predictedTargetE0;

        immediate <= immE0; // main immediate that will be used in E1
        instruction <= insTypeE0; // main instruction that will be used in E1
        valueAE1 <= (firstLoadOccured) ? stalled_vA + 2 : vAForwarded; // main first reg's value that will be used in E1
        valueBE1 <= (firstLoadOccured) ? stalled_vB : vBForwarded; // main second reg's value that will be used in E1

        firstSrcRegE1 <= firstSrcRegE0;
        secondSrcRegE1 <= secondSrcRegE0;
        destRegE1 <= destRegE0;
        loadMisalignmentOccured <= misalignedLoadDetected || firstLoadOccured;

        if (misalignedLoadDetected) begin
            firstLoadOccured <= 1;
            stalled_vA <= vAForwarded;
            stalled_vB <= vBForwarded;
        end else begin
            firstLoadOccured <= 0;
        end

    end

    /**********
     * Execute (Part 1) *
     **********/

    // checking if any of these values were read from register 0
    wire [15:0] vA = (firstSrcRegE1 == 0) ? 16'b0 : valueAE1;
    wire [15:0] vB = (secondSrcRegE1 == 0) ? 16'b0 : valueBE1;

     // implements forwarding from wb -> exec1
    wire canForwardExec_1_WB = (wb_v && (instructionForWB[0] || instructionForWB[1] || instructionForWB[2] || instructionForWB[7])); // basically same logic as wb->exec_0
    wire forwardRegA_E1_WB = canForwardExec_1_WB && (destRegWB != 0) && (destRegWB == firstSrcRegE1);
    wire forwardRegB_E1_WB = canForwardExec_1_WB && (destRegWB != 0) && (destRegWB == secondSrcRegE1);

    wire [15:0] vAE1Temp =  (forwardRegA_E1_WB) ? writeData :  
                        vA;
    wire [15:0] vBE1Temp =  (forwardRegB_E1_WB) ? writeData : 
                        vB;

    // another set of forwarding solely for load after store case (if vAE1Temp == vAForStore)
    // only need to check for register A
    reg [15:0] vAForStore; // stores value of register A; solely for the store instruction in write_back
    wire forwardStoreVal =  (strEN && instruction[7] && ~loadMisalignmentOccured) && (vAE1Temp == vAForStore);
                            // only forwards store value if current instruction is load (for aligned case)

    // load after store forwarding cases
    wire forwardStoreLoadBothMisaligned = ((strEN && instruction[7] && loadMisalignmentOccured) 
            && (vAE1Temp) == vAForStore); // both are misaligned 
    wire forwardAlignedStoreMisalignedLoad = ((strEN && instruction[7] && loadMisalignmentOccured) 
            && (vAE1Temp - 1) == vAForStore); // Aligned str - > Misaligned ldr
    wire forwardMisalignedStoreAlignedLoad = ((strEN && instruction[7] && thereWasMisalignedLoad) 
            && (vAE1Temp + 1) == vAForStore); // Misaligned str -> Aligned ldr 

    wire forwardingBecauseOfStoreToLoadHazard = (forwardStoreVal | forwardStoreLoadBothMisaligned 
            | forwardAlignedStoreMisalignedLoad | forwardMisalignedStoreAlignedLoad);

    // store after store forwarding cases
    wire forwardAlignedStoreToMisalignedStore = (strEN && instruction[8] && loadMisalignmentOccured) 
                                             && (vAE1Temp - 1 == vAForStore);

    wire forwardMisalignedStoreToMisalignedStore = (strEN && instruction[8] && loadMisalignmentOccured 
            && usingSecondLoad) && (vAE1Temp == vAForStore);

    wire forwardingBecauseOfStoreToStoreHazard = (forwardAlignedStoreToMisalignedStore 
            | forwardMisalignedStoreToMisalignedStore);

    wire [15:0] vAE1 = (forwardingBecauseOfStoreToLoadHazard) ? writeData : vAE1Temp;
    wire [15:0] vBE1 = (forwardingBecauseOfStoreToStoreHazard) ? {vBE1Temp[7:0], writeData[7:0]} : vBE1Temp;

    // Perform ALU operations based on the decoded opcode
    assign result = (instruction[0]) ? (vAE1 - vBE1) :
                    (instruction[1]) ? {{8 {immediate[7]}}, immediate[7:0]} : // sign-extends to 16 bits
                    (instruction[2]) ? (vBE1 & 16'h00FF) | (immediate << 8) : // vB stores value read from rT
                    (instruction[3] || instruction[4] || instruction[5] || instruction[6]) ? (vBE1) : // address to branch too
                    (instruction[7]) ? (vAE1) : // address load is retrieving from
                    (instruction[8]) ? (vBE1) : // store's value is just the original rT's value
                    16'bX; // unknown value

    wire canJump;
    assign canJump =  ((instruction[3] && (vAE1 == 0)) ? 1'b1 : 
                        (instruction[4] && (vAE1 != 0)) ? 1'b1 : 
                        (instruction[5] && ($signed(vAE1) < 0)) ? 1'b1 : 
                        (instruction[6] && ($signed(vAE1) >= 0)) ? 1'b1 : 
                        1'b0); // used to detect if this instruction's branch condition will return true

    // these regs will be used in write_back stage
    reg tempCanJump; // stores the canJump flag for the write back stage
    reg useForwardedVal; // passed into write_back so that vaE1 can be the "loaded value" instead of the "loadedData" wire for memory hazards
    reg [15:0] misalignedLoadValue; // will be used in write back if there was a misaligned load
    reg thereWasMisalignedLoad;
    always @(posedge clk) begin
        pcWB <= pcE1;
        predictJumpWB <= predictJumpE1;
        predictedTargetWB <= predictedTargetE1;

        instructionForWB <= instruction;
        writeDataReg <= result;
        tempCanJump <= canJump;
        destRegWB <= destRegE1;
        vAForStore <= vAE1; // solely used for the store instruction in write_back stage
        useForwardedVal <= forwardingBecauseOfStoreToLoadHazard || forwardingBecauseOfStoreToStoreHazard; // solely used for load instruction in write_back stage
        thereWasMisalignedLoad <= loadMisalignmentOccured;
    end

    /**********
     * Write back (Part 0) *
     **********/


    // computing if write enabled using instructionForWB wire
    assign wEN = wb_v && (instructionForWB[0] || instructionForWB[1] || instructionForWB[2] ||
            instructionForWB[7]) && (~thereWasMisalignedLoad || usingSecondLoad);

    wire [15:0] writeDataTemp =  (instructionForWB[7] && ~useForwardedVal) ? loadedData :
                            writeDataReg; // data from load instruction should be ready by start of write back

    wire [15:0] combinedLoadValue = (usingSecondLoad) ? {writeDataTemp[7:0], misalignedLoadValue[15:8]} : 
                                    writeDataTemp; // for misaligned load instruction
    
    // storing writeDataReg in the 2 addresses for misaligned stores
    wire [15:0] combinedStoreValue_1 = (instructionForWB[8] && thereWasMisalignedLoad && useForwardedVal) ?
                                       writeDataReg : {writeDataReg[7:0], loadedData[7:0]}; 
    wire [15:0] combinedStoreValue_2 = {loadedData[15:8], writeDataReg[15:8]};

    assign writeData =  (instructionForWB[8] && thereWasMisalignedLoad) ?
                            ((~usingSecondLoad) ? combinedStoreValue_1 : combinedStoreValue_2)
                        : combinedLoadValue;

    assign isJumping = wb_v && tempCanJump; // determining if pc can be branched during write back

    assign isValidBranchIns = wb_v && (instructionForWB[3] || instructionForWB[4] || instructionForWB[5] ||
            instructionForWB[6]);
    
    assign incorrectPrediction = wb_v && ((isJumping && !(predictJumpWB && (predictedTargetWB == writeData))) 
            || (~isJumping && predictJumpWB)); // can also be true if instruction doesn't jump (because it's not a branch or branch condition returns false), but the predictor made a jump occur

    // store implementation
    assign strEN = wb_v && instructionForWB[8];
    assign storeAddress = vAForStore; // value of register A is memory address where data is being stored
    assign storedData = writeData;

    wire [15:0] addressBeingSelfModified = (usingSecondLoad) ? storeAddress - 2
                                            : storeAddress;


    // during misaligned self-modifying, since we obtain the loaded data, we can save cycles by not self modifying if the value in the memory address doesn't change
    wire writingSameData = (strEN && thereWasMisalignedLoad && writeData == loadedData);
    reg wroteSameDataForMisalignedStore;

    assign selfModifyingStage = (addressBeingSelfModified >= pcE1 - 1 && addressBeingSelfModified <= pcE1 + 1) ?
                                    5 : // flush wb
                                    (addressBeingSelfModified >= pcE0 - 1
                                         && addressBeingSelfModified <= pcE0 + 1) ? 4 : // flush e1
                                    (addressBeingSelfModified >= pcDC - 1 
                                        && addressBeingSelfModified <= pcDC + 1) ? 
                                    ((isStallingDecode) ? 2 : 3) : // flush dc/e0 (depends on isStallingDecode)
                                    (isMisaligned && addressBeingSelfModified >= pcForDecodeIfMisaligned - 1 
                                        && addressBeingSelfModified <= pcForDecodeIfMisaligned + 1) ? 2 :
                                    (addressBeingSelfModified >= pcF1 - 1 
                                        && addressBeingSelfModified <= pcF1 + 1) ? 2 : // flush dc
                                    (addressBeingSelfModified >= pc - 1 
                                        && addressBeingSelfModified <= pc + 1) ? 1 : // flush f1
                                    6;
    assign isSelfModifying = (strEN && 
            (selfModifyingStage >= 1 && selfModifyingStage <= 5)) 
            && (~thereWasMisalignedLoad || usingSecondLoad) 
            && !(wroteSameDataForMisalignedStore && writingSameData); // only self modifies if written memory address's value was actually changed

    // for partial flushing during self-modifying code
    wire [15:0] pcToRestartFrom = (addressBeingSelfModified >= pcE1 - 1 
                                    && addressBeingSelfModified <= pcE1 + 1) ? pcE1 :
                                    (addressBeingSelfModified >= pcE0 - 1 
                                        && addressBeingSelfModified <= pcE0 + 1) ? pcE0 :
                                    (addressBeingSelfModified >= pcDC - 1 
                                        && addressBeingSelfModified <= pcDC + 1) ? 
                                    ((isStallingDecode) ? pcDC : pcDC) :
                                    (isMisaligned && addressBeingSelfModified >= pcForDecodeIfMisaligned - 1 
                                        && addressBeingSelfModified <= pcForDecodeIfMisaligned + 1) ? pcForDecodeIfMisaligned :
                                    (addressBeingSelfModified >= pcF1 - 1 
                                        && addressBeingSelfModified <= pcF1 + 1) ? pcF1 :
                                    (addressBeingSelfModified >= pc - 1 
                                        && addressBeingSelfModified <= pc + 1) ? pc : 
                                    pcWB + 2;

    always @(posedge clk) begin 
        if (thereWasMisalignedLoad && !usingSecondLoad) begin // used to write back twice for misaligned loads/stores
            usingSecondLoad <= 1;
            misalignedLoadValue <= combinedLoadValue;
            wroteSameDataForMisalignedStore <= writingSameData;
        end else begin 
            usingSecondLoad <= 0;
            wroteSameDataForMisalignedStore <= 0;
        end

        if (exec1_v && ~incorrectPrediction && instruction == 0) begin // halt if invalid instruction and write_back is not branching
            halt <= 1;
        end
        if (destRegWB == 0 && wEN) begin // printing to system if destination register is 0
            $write("%c", writeData[7:0]);
        end

        if (incorrectPrediction || isSelfModifying || !isStallingF0) begin
            pc <= (isSelfModifying) ? pcToRestartFrom : // reset PC to instruction right after self-modifying store
                  (incorrectPrediction && ~isJumping) ? pcWB + 2 : 
                  (incorrectPrediction && isJumping) ? writeData :
                  (predictJump) ?
                    ((pc[0]) ? // need to make sure next word is read before misaligned PC prediction occurs
                        pc + 2 : predictedTarget) : 
                  (pcToJumpFromWasMisaligned) ? // subtract 2 from prediction if jumping to misaligned PC
                    ((targetPredictionWasMisaligned) ? predictedTargetFromMisalignedPC - 2 : 
                    predictedTargetFromMisalignedPC) :
                  pc + 2; 
        end
    end

endmodule