#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <string>

#include "MemoryStore.h"
#include "RegisterInfo.h"

// --------------------------------------------------------------------------
// Reg data structure
// --------------------------------------------------------------------------

union REGS {
    RegisterInfo reg;
    uint64_t registers[REG_SIZE] {0};
};

union REGS regData;

uint64_t PC;

// --------------------------------------------------------------------------
// Decode constants
// --------------------------------------------------------------------------

// TODO: Complete these enums, and add more if necessary

enum OPCODES {
    // I-type opcodes
    OP_INTIMM  = 0b0010011, // Integer ALU immediate instructions addi, slli, slti, sltiu, xori, srli, srai, ori, andi
    OP_LOAD = 0b0000011, // for instructions lb, lh, lw, ld, lbu, lhu, lwu
    OP_STORE = 0b0100011, // for instructions sb, sh, sw, sd
    OP_RINT = 0b0100111, // for instructions add, sub, sli, slt, sltu, xor, srl, sra, or, and
    OP_LUI = 0b0110111, // lui
    OP_RW = 0b0111011, // addw, subw, sllw, srlw, sraw
    OP_SB = 0b1100011, // bew, bne, blt, bge, bltu, bgeu
    OP_I = 0b1100111, // jalr
    OP_UJ = 0b1101111, //jal
    OP_IMMW = 0b0011011, // addiw, slliw, srliw, sraiw
    OP_AUIPC = 0b0010111 // auipc
    // ...
};

enum FUNCT3 {
    // For integer ALU instructions
    FUNCT3_BEQ  = 0b000,
    FUNCT3_BNE  = 0b001,
    FUNCT3_BLT  = 0b100,
    FUNCT3_BGE  = 0b101,
    FUNCT3_BLTU = 0b110,
    FUNCT3_BGEU = 0b111,
    FUNCT3_LB_SB = 0b000,
    FUNCT3_LH_SH = 0b001,
    FUNCT3_LW_SW = 0b010,
    FUNCT3_LD_SD = 0b011,
    FUNCT3_LBU = 0b100,
    FUNCT3_LHU = 0b101,
    FUNCT3_LWU = 0b110,
    FUNCT3_ADDI = 0b000,
    FUNCT3_SLLI = 0b001,
    FUNCT3_SLTI = 0b010,
    FUNCT3_SLTIU = 0b011,
    FUNCT3_XORI = 0b100,
    FUNCT3_SRLI = 0b101,
    FUNCT3_SRAI = 0b101,
    FUNCT3_ORI = 0b110,
    FUNCT3_ANDI = 0b111,
    FUNCT3_ADDIW = 0b000,
    FUNCT3_SLLIW = 0b001,
    FUNCT3_SRLIW = 0b101,
    FUNCT3_SRAIW = 0b101,
    FUNCT3_ADD = 0b000,
    FUNCT3_SUB = 0b000,
    FUNCT3_SLL = 0b001,
    FUNCT3_SLT = 0b010,
    FUNCT3_SLTU = 0b011,
    FUNCT3_XOR = 0b100,
    FUNCT3_SRL = 0b101,
    FUNCT3_SRA = 0b101,
    FUNCT3_OR = 0b110,
    FUNCT3_AND = 0b111
    // ...
};

enum RI_FUNCT7 {
    // For R-type add/sub instructions
    FUNCT7_ADD     = 0b0000000, // add
    FUNCT7_SUB     = 0b0100000, // sub, subw
    FUNCT7_SRLI    = 0b0000000, // srl, srlw
    FUNCT7_SRAI    = 0b0100000, // srai, sraiw
    FUNCT7_SRLIW   = 0b0000000, // srlw
    FUNCT7_SRAIW   = 0b0100000 // sraw
    // ...
};

// --------------------------------------------------------------------------
// Bit-level manipulation helpers
// --------------------------------------------------------------------------


// TODO: You may wish to declare some helper functions for bit extraction
// and sign extension

uint64_t sign_extend_32(uint64_t val);

// --------------------------------------------------------------------------
// Utilities
// --------------------------------------------------------------------------

// Initialize memory with the program binary
bool initMemory(char *programFile, MemoryStore *myMem);

// Dump registers and memory
void dump(MemoryStore *myMem);

// --------------------------------------------------------------------------
// Simulation functions
// --------------------------------------------------------------------------

// The simulator maintains the following struct as it simulates
// RISC-V instructions. Feel free to add more fields if needed.
struct Instruction {
    uint64_t PC = 0;
    uint64_t instruction = 0; // raw instruction binary

    bool     isHalt = false;
    bool     isLegal = false;
    bool     isNop = false;

    bool     readsMem = false;
    bool     writesMem = false;
    bool     doesArithLogic = false;
    bool     writesRd = false;
    bool     readsRs1 = false;
    bool     readsRs2 = false;

    uint64_t opcode = 0;
    uint64_t funct3 = 0;
    uint64_t funct7 = 0;
    uint64_t rd = 0;
    uint64_t rs1 = 0;
    uint64_t rs2 = 0;

    uint64_t nextPC = 0;
    uint64_t branchAddr = 0;
    uint64_t imm_I = 0;
    uint64_t imm_UJ = 0;
    uint64_t imm_SB = 0;
    uint64_t imm_U = 0;

    uint64_t op1Val = 0;
    uint64_t op2Val = 0;

    uint64_t arithResult = 0;
    uint64_t memAddress = 0;
    uint64_t memResult = 0;
};

// The following functions are the core of the simulator. Your task is to
// complete these functions in sim.cpp. Do not modify their signatures.
// However, feel free to declare more functions if needed.

// There is no strict rule on what each function should do, but the
// following comments give suggestions.

// Fetch the raw instruction bits from memory
Instruction simFetch(uint64_t PC, MemoryStore *myMem);

// Decode the opcode, funct fields, and register indices, and determine which resources to use
Instruction simDecode(Instruction inst);

// Collect register operands for arithmetic or address generation
Instruction simOperandCollection(Instruction inst, REGS regData);

// Resolve the next PC: PC + 4, or a branch/jump target
Instruction simNextPCResolution(Instruction inst);

// Perform arithmetic/logic operations
Instruction simArithLogic(Instruction inst);

// Generate the memory address for load/store instructions
Instruction simAddrGen(Instruction inst);

// Perform the memory access for load/store instructions
Instruction simMemAccess(Instruction inst, MemoryStore *myMem);

// Write results back to the register file
Instruction simCommit(Instruction inst, REGS &regData);

// Simulate one whole instruction using the functions above
Instruction simInstruction(uint64_t &PC, MemoryStore *myMem, REGS &regData);

#endif // SIM_H
