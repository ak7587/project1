#include "sim.h"

using namespace std;

// RV64I without CSR, environment, or fence instructions

//           31          25 24 20 19 15 14    12 11          7 6      0
// R  type: | funct7       | rs2 | rs1 | funct3 | rd          | opcode |
// I  type: | imm[11:0]          | rs1 | funct3 | rd          | opcode |
// S  type: | imm[11:5]    | rs2 | rs1 | funct3 | imm[4:0]    | opcode |
// SB type: | imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode |
// U  type: | imm[31:12]                        | rd          | opcode |
// UJ type: | imm[20|10:1|11|19:12]             | rd          | opcode |

uint64_t sign_extend_32(uint64_t val) {
    return static_cast<int64_t>(static_cast<int32_t>(val));
}
// Initialize memory with the program binary
bool initMemory(char *programFile, MemoryStore *myMem) {
    // Open the instruction file
    ifstream infile;
    infile.open(programFile, ios::binary | ios::in);

    if (!infile.is_open()) {
        fprintf(stderr, "\tError open input file\n");
        return false;
    }

    // Get the length of the file and read the instruction file into a buffer
    infile.seekg(0, ios::end);
    int length = infile.tellg();
    infile.seekg (0, ios::beg);

    char *buf = new char[length];
    infile.read(buf, length);
    infile.close();

    int memLength = length / sizeof(buf[0]);
    int i;
    for (i = 0; i < memLength; i++) {
        myMem->setMemValue(i * BYTE_SIZE, buf[i], BYTE_SIZE);
    }

    return true;
}

// Dump registers and memory
void dump(MemoryStore *myMem) {

    dumpRegisterState(regData.reg);
    dumpMemoryState(myMem);
}

// TODO: Complete the functions below. Refer to the project description.
// This template provides an implementation of the ADDI instruction.

// Fetch the raw instruction bits from memory
Instruction simFetch(uint64_t PC, MemoryStore *myMem) {
    // Fetch the current instruction
    uint64_t instruction;
    myMem->getMemValue(PC, instruction, WORD_SIZE);
    instruction = (uint32_t)instruction;

    Instruction inst;
    inst.PC = PC;
    inst.instruction = instruction;
    return inst;
}

// Decode the opcode, funct fields, and register indices, and determine which resources to use
Instruction simDecode(Instruction inst) {
    inst.opcode = inst.instruction & 0b1111111;
    inst.funct3 = inst.instruction >> 12 & 0b111;
    inst.rd = inst.instruction >> 7 & 0b11111;
    inst.rs1 = inst.instruction >> 15 & 0b11111;
    inst.rs2 = inst.instruction >> 20 & 0b11111;
    inst.funct7 = inst.instruction >> 25 & 0b1111111;
    inst.branchAddr = inst.instruction >> 25 & inst.instruction;

    //unsigned immediates
    uint64_t imm_I = inst.instruction >> 20 & 0b111111111111;
    uint64_t imm_UJ = (((inst.instruction >> 31 & 0b1) << 10 | (inst.instruction >> 12 & 0b1111111111)) << 1 | (inst.instruction >> 22 & 0b1)) << 8 | (inst.instruction >> 23 & 0b11111111);
    uint64_t imm_SB = (((inst.instruction >> 31 & 0b1) << 1 | (inst.instruction >> 7 & 0b1)) << 6 | (inst.instruction >> 25 & 0b111111)) << 4 | (inst.instruction >> 8 & 0b1111);
    uint64_t imm_U = inst.instruction >> 12 & 0b11111111111111111111;
    
    // Sign-extend the immediate values & shift the branch/jump immediates to the left by 1 bit
    inst.imm_SB = ((imm_SB & 0x800) ? (imm_SB | 0xFFFFFFFFFFFFF000) : imm_SB) << 1;
    inst.imm_I = (imm_I & 0x800) ? (imm_I | 0xFFFFFFFFFFFFF000) : imm_I;
    inst.imm_UJ = ((imm_UJ & 0x80000) ? (imm_UJ | 0xFFFFFFFFFFF00000) : imm_UJ) << 1;
    inst.imm_U = (imm_U & 0x80000) ? (imm_U | 0xFFFFFFFFFFF00000) : imm_U;
   
    inst.isLegal = true; // assume legal unless proven otherwise

    if (inst.instruction == 0xfeedfeed) {
        inst.isHalt = true;
        return inst; // halt instruction
    }

    if (inst.instruction == 0x00000013) {
        inst.isNop = true;
        return inst; // NOP instruction
    }

    switch (inst.opcode) {
        case OP_INTIMM:
            if (inst.funct3 == FUNCT3_ADD) {
                inst.doesArithLogic = true;
                inst.writesRd = true;
                inst.readsRs1 = true;
                inst.readsRs2 = false;
            } else {
                inst.isLegal = false;
            }
            break;
        default:
            inst.isLegal = false;
    }
    return inst;
}

// Collect register operands for arithmetic or address generation
Instruction simOperandCollection(Instruction inst, REGS regData) {

    inst.op1Val = regData.registers[inst.rs1];
    inst.op2Val = regData.registers[inst.rs2];

    return inst;
}


// Resolve the next PC: PC + 4, or a branch/jump target
Instruction simNextPCResolution(Instruction inst) {
    if (inst.opcode == OP_SB || inst.opcode == OP_I || inst.opcode == OP_UJ) {
        switch (inst.opcode) {
            case OP_I:
                registers[inst.rd] = inst.PC + 4;
                inst.nextPC = inst.op1Val + inst.imm_I;
                break;
            case OP_UJ:
                registers[inst.rd] = inst.PC + 4;
                inst.nextPC = inst.PC + inst.imm_UJ;
                break;
            case OP_SB:
                switch (inst.funct3) {
                    case FUNCT3_BEQ:
                        if (inst.rs1 == inst.rs2) {
                            inst.nextPC = inst.PC + inst.imm_SB;
                        }
                        else inst.nextPC = inst.PC;
                        break;
                    case FUNCT3_BGE:
                        if (inst.rs1 >= inst.rs2) {
                            inst.nextPC = inst.PC + inst.imm_SB;
                        }
                        else inst.nextPC = inst.PC;
                        break;
                    case FUNCT3_BLT:
                        if (inst.rs1 < inst.rs2) {
                            inst.nextPC = inst.PC + inst.imm_SB;
                        }
                        else inst.nextPC = inst.PC;
                        break;
                    case FUNCT3_BNE:
                        if (inst.rs1 != inst.rs2) {
                            inst.nextPC = inst.PC + inst.imm_SB;
                        }
                        else inst.nextPC = inst.PC;
                        break;
                    case FUNCT3_BLTU:
                        if (inst.rs1 < inst.rs2) {
                            inst.nextPC = inst.PC + inst.imm_SB;
                        }
                        else inst.nextPC = inst.PC;
                        break;
                    case FUNCT3_BGEU:
                        if (inst.rs1 >= inst.rs2) {
                            inst.nextPC = inst.PC + inst.imm_SB;
                        }
                        else inst.nextPC = inst.PC;
                        break;
                }
                break;
        }
    }
    else {
        inst.nextPC = inst.PC + 4;
    }

    return inst;
}

// Perform arithmetic/logic operations
Instruction simArithLogic(Instruction inst) {
    if (inst.opcode == OP_RINT) {
        switch (inst.funct3) {
            case FUNCT3_ADD:
                if (inst.funct7 == FUNCT7_SUB) {
                    inst.arithResult = inst.op1Val - inst.op2Val;
                } else {
                    inst.arithResult = inst.op1Val + inst.op2Val;
                }
                break;
            case FUNCT3_SLL:
                inst.arithResult = inst.op1Val << (inst.op2Val & 0x3F);
                break;
            case FUNCT3_SRL:
                // For SRL and SRA, we need to check the funct7 field to determine if it's a logical or arithmetic shift
                if (inst.funct7 == FUNCT7_SUB) {
                    inst.arithResult = ((int64_t)inst.op1Val) >> (inst.op2Val & 0x3F);
                } else {
                    inst.arithResult = inst.op1Val >> (inst.op2Val & 0x3F);
                }
                break;
            case FUNCT3_AND:
                inst.arithResult = inst.op1Val & inst.op2Val;
                break;
            case FUNCT3_OR:
                inst.arithResult = inst.op1Val | inst.op2Val;
                break;
            case FUNCT3_XOR:
                inst.arithResult = inst.op1Val ^ inst.op2Val;
                break;
            case FUNCT3_SLT:
                inst.arithResult = ((int64_t)inst.op1Val < (int64_t)inst.op2Val) ? 1 : 0;
                break;
            case FUNCT3_SLTU:
                inst.arithResult = (inst.op1Val < inst.op2Val) ? 1 : 0;
                break;
        }
    } else if (inst.opcode == OP_INTIMM) {
        switch (inst.funct3) {
            case FUNCT3_ADDI:
                inst.arithResult = inst.op1Val + inst.imm_I;
                break;
            case FUNCT3_SLLI:
                inst.arithResult = inst.op1Val << (inst.imm_I & 0x3F);
                break;
            case FUNCT3_SLTI:
                inst.arithResult = ((int64_t)inst.op1Val < (int64_t)inst.imm_I) ? 1 : 0;
                break;
            case FUNCT3_SLTIU:
                inst.arithResult = (inst.op1Val < inst.imm_I) ? 1 : 0;
                break;
            case FUNCT3_XORI:
                inst.arithResult = inst.op1Val ^ inst.imm_I;
                break;
            case FUNCT3_SRLI:
                if (inst.funct7 == FUNCT7_SRAI) {
                    inst.arithResult = ((int64_t)inst.op1Val) >> (inst.imm_I & 0x3F);
                } else {
                    inst.arithResult = inst.op1Val >> (inst.imm_I & 0x3F);
                }
                break;
            case FUNCT3_ORI:
                inst.arithResult = inst.op1Val | inst.imm_I;
                break;
            case FUNCT3_ANDI:
                inst.arithResult = inst.op1Val & inst.imm_I;
                break;
        }
    } else if (inst.opcode == OP_IMMW) {
        switch (inst.funct3) {
            case FUNCT3_ADDIW:
                inst.arithResult = sign_extend_32(inst.op1Val + inst.imm_I);
                break;
            case FUNCT3_SLLIW:
                inst.arithResult = sign_extend_32(inst.op1Val << (inst.imm_I & 0x1F));
                break;
            case FUNCT3_SRLIW:
                if (inst.funct7 == FUNCT7_SRAIW) {
                    inst.arithResult = sign_extend_32(((int32_t)inst.op1Val) >> (inst.imm_I & 0x1F));
                } else {
                    inst.arithResult = sign_extend_32(inst.op1Val >> (inst.imm_I & 0x1F));
                }
                break;
        }
    } else if (inst.opcode == OP_RINT) {
        switch (inst.funct3) {
            case FUNCT3_ADD:
                if (inst.funct7 == FUNCT7_SUB) {
                    inst.arithResult = inst.op1Val - inst.op2Val;
                } else {
                    inst.arithResult = inst.op1Val + inst.op2Val;
                }
                break;
            case FUNCT3_SLL:
                inst.arithResult = inst.op1Val << (inst.op2Val & 0x1F);
                break;
            case FUNCT3_SRL:
                if (inst.funct7 == FUNCT7_SRA) {
                    inst.arithResult = ((int64_t)inst.op1Val) >> (inst.op2Val & 0x1F);
                } else {
                    inst.arithResult = inst.op1Val >> (inst.op2Val & 0x1F);
                }
                break;
        }
    } else if (inst.opcode == OP_LUI | inst.opcode == OP_AUIPC) {
        switch (inst.opcode) {
            case OP_LUI:
                inst.arithResult = inst.imm_U << 12;
                break;
            case OP_AUIPC:
                inst.arithResult = inst.PC + (inst.imm_U << 12);
                break;
        }
    }
    
    // Only ADDI is implemented; handle the other opcodes here
    uint64_t imm12  = inst.instruction >> 20 & 0b111111111111;
    uint64_t sext_imm12 = (imm12 & 0x800) ? (imm12 | 0xFFFFFFFFFFFFF000) : imm12;
    inst.arithResult = inst.op1Val + sext_imm12;

    return inst;
}

// Generate the memory address for load/store instructions
Instruction simAddrGen(Instruction inst) {
    inst.memAddress = inst.op1Val + inst.imm_I;
    return inst;
}

// Perform the memory access for load/store instructions
Instruction simMemAccess(Instruction inst, MemoryStore *myMem) {
    uint64_t value;
if (inst.opcode == OP_LOAD) {
    switch (inst.funct3) {
        case FUNCT3_LB_SB:
            myMem->getMemValue(inst.memAddress, value, BYTE_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
        case FUNCT3_LH_SH:
            myMem->getMemValue(inst.memAddress, value, HALF_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
        case FUNCT3_LW_SW:
            myMem->getMemValue(inst.memAddress, value, WORD_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
        case FUNCT3_LD_SD:
            myMem->getMemValue(inst.memAddress, value, DOUBLE_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
        case FUNCT3_LBU:
            myMem->getMemValue(inst.memAddress, value, BYTE_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
        case FUNCT3_LHU:
            myMem->getMemValue(inst.memAddress, value, HALF_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
        case FUNCT3_LWU:
            myMem->getMemValue(inst.memAddress, value, WORD_SIZE);
            registers[inst.rd] = (int64_t)value;
            break;
    }
    if (inst.opcode == OP_STORE) {
        switch (inst.funct3) {
            case FUNCT3_LB_SB:
                value = registers[inst.rd] & 0xFF;
                myMem->setMemValue(inst.memAddress, value, BYTE_SIZE);
                break;
            case FUNCT3_LH_SH:
                value = registers[inst.rd] & 0xFFFF;
                myMem->setMemValue(inst.memAddress, value, HALF_SIZE);
                break;
            case FUNCT3_LW_SW:
                value = registers[inst.rd] & 0xFFFFFFFF;
                myMem->setMemValue(inst.memAddress, value, WORD_SIZE);
                break;
            case FUNCT3_LD_SD:
                value = registers[inst.rd];
                myMem->setMemValue(inst.memAddress, value, DOUBLE_SIZE);
                break;
            }
        }
    }
    return inst;
}

// Write results back to the register file
Instruction simCommit(Instruction inst, REGS &regData) {

    // regData is passed by reference, so changes here update the caller's registers
    regData.registers[inst.rd] = inst.arithResult;

    regData.registers[0] = 0;

    return inst;
}

// Simulate one whole instruction using the functions above
Instruction simInstruction(uint64_t &PC, MemoryStore *myMem, REGS &regData) {
    Instruction inst = simFetch(PC, myMem);
    inst = simDecode(inst);
    if (!inst.isLegal || inst.isHalt) return inst;
    inst = simOperandCollection(inst, regData);
    inst = simNextPCResolution(inst);
    inst = simArithLogic(inst);
    inst = simAddrGen(inst);
    inst = simMemAccess(inst, myMem);
    inst = simCommit(inst, regData);
    PC = inst.nextPC;
    return inst;
}

int main(int argc, char** argv) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <instruction_file>\n", argv[0]);
        return -1;
    }

    // Initialize the memory store with the buffer contents
    MemoryStore *myMem = createMemoryStore();
    if (!initMemory(argv[1], myMem)) {
        fprintf(stderr, "Failed to initialize memory with program binary.\n");
        return -1;
    }

    // Initialize registers and the program counter
    regData.reg = {};
    PC = 0;
    bool err = false;

    // Start the simulation
    while (!err) {
        Instruction inst = simInstruction(PC, myMem, regData);
        if (inst.isHalt) {
            // Halt: dump state and exit normally
            dump(myMem);
            return 0;
        }
        if (!inst.isLegal) {
            fprintf(stderr, "Illegal instruction encountered at PC: 0x%lx\n", inst.PC);
            err = true;
        }
    }

    // Illegal instruction: dump state and exit with an error
    dump(myMem);
    exit(127);
    return -1;
}
