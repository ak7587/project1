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

    return inst;
}

// Resolve the next PC: PC + 4, or a branch/jump target
Instruction simNextPCResolution(Instruction inst) {

    inst.nextPC = inst.PC + 4;

    return inst;
}

// Perform arithmetic/logic operations
Instruction simArithLogic(Instruction inst) {
    // Only ADDI is implemented; handle the other opcodes here
    uint64_t imm12  = inst.instruction >> 20 & 0b111111111111;
    uint64_t sext_imm12 = (imm12 & 0x800) ? (imm12 | 0xFFFFFFFFFFFFF000) : imm12;

    inst.arithResult = inst.op1Val + sext_imm12;

    return inst;
}

// Generate the memory address for load/store instructions
Instruction simAddrGen(Instruction inst) {
    return inst;
}

// Perform the memory access for load/store instructions
Instruction simMemAccess(Instruction inst, MemoryStore *myMem) {
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
