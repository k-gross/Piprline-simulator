#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Machine Definitions
#define NUMMEMORY 65536 // maximum number of data words in memory
#define NUMREGS 8 // number of machine registers

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 // will not implemented for Project 3
#define HALT 6
#define NOOP 7

const char* opcode_to_str_map[] = {
    "add",
    "nor",
    "lw",
    "sw",
    "beq",
    "jalr",
    "halt",
    "noop"
};

#define NOOPINSTRUCTION (NOOP << 22)

typedef struct IFIDStruct {
	int instr;
	int pcPlus1;
} IFIDType;

typedef struct IDEXStruct {
	int instr;
	int pcPlus1;
	int readRegA;
	int readRegB;
	int offset;
} IDEXType;

typedef struct EXMEMStruct {
	int instr;
	int branchTarget;
    int eq;
	int aluResult;
	int readRegB;
} EXMEMType;

typedef struct MEMWBStruct {
	int instr;
	int writeData;
} MEMWBType;

typedef struct WBENDStruct {
	int instr;
	int writeData;
} WBENDType;

typedef struct stateStruct {
	int pc;
	int instrMem[NUMMEMORY];
	int dataMem[NUMMEMORY];
	int reg[NUMREGS];
	int numMemory;
	IFIDType IFID;
	IDEXType IDEX;
	EXMEMType EXMEM;
	MEMWBType MEMWB;
	WBENDType WBEND;
	int cycles; // number of cycles run so far
} stateType;

static inline int opcode(int instruction) {
    return instruction>>22;
}

static inline int field0(int instruction) {
    return (instruction>>19) & 0x7;
}

static inline int field1(int instruction) {
    return (instruction>>16) & 0x7;
}

static inline int field2(int instruction) {
    return instruction & 0xFFFF;
}

// convert a 16-bit number into a 32-bit Linux integer
static inline int convertNum(int num) {
    return num - ( (num & (1<<15)) ? 1<<16 : 0 );
}

void printState(stateType*);
void printInstruction(int);
void readMachineCode(stateType*, char*);
void calculateRegisters(stateType*, int*, int*);
int calculateALU(int, int, int, int, int);



int main(int argc, char *argv[]) {

    /* declare our state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    static stateType state, newState;
    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    
    state.IDEX.instr = NOOPINSTRUCTION;
    state.IFID.instr = NOOPINSTRUCTION;
    state.EXMEM.instr = NOOPINSTRUCTION;
    state.MEMWB.instr = NOOPINSTRUCTION;
    state.WBEND.instr = NOOPINSTRUCTION;
    
    for (int j = 0; j < NUMREGS; ++j) {
        state.reg[j] = 0;
    }

    state.cycles = 0;
    state.pc = 0;

    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        newState = state;
        newState.cycles++;

        /* ---------------------- IF stage --------------------- */
        newState.IFID.instr = state.instrMem[state.pc];
        newState.pc = newState.pc+1;
        newState.IFID.pcPlus1 = state.pc+1;

        /* ---------------------- ID stage --------------------- */
        int offset = field2(state.IFID.instr);
        int regA = field0(state.IFID.instr);
        int regB = field1(state.IFID.instr);

        //Data hazards that do involve stalls
        //Look for LW followed by instruction that uses register being loaded
        newState.IDEX.instr = state.IFID.instr;
        if (opcode(state.IDEX.instr) == LW) {
            if (regA == field1(state.IDEX.instr) || regB == field1(state.IDEX.instr)) {
                newState.pc = state.pc;
                //Dont update pc if you stall
                newState.IFID = state.IFID;
                newState.IDEX.instr = NOOPINSTRUCTION;

            }
        }
      
        newState.IDEX.offset = convertNum(offset);
        newState.IDEX.pcPlus1 = state.IFID.pcPlus1;
        newState.IDEX.readRegA = state.reg[regA];
        newState.IDEX.readRegB = state.reg[regB];

        

        /* ---------------------- EX stage --------------------- */
        int regA2 = state.IDEX.readRegA;
        int regB2 = state.IDEX.readRegB;
 
        calculateRegisters(&state, &regA2, &regB2);
        int ALU = calculateALU(state.IDEX.instr, regA2, regB2, state.IDEX.offset, state.IDEX.pcPlus1);

        int branch = state.IDEX.pcPlus1 + state.IDEX.offset;


        newState.EXMEM.instr = state.IDEX.instr;
        newState.EXMEM.aluResult = ALU;
        newState.EXMEM.branchTarget = branch;
        newState.EXMEM.eq = (regA2 == regB2);
        //newState.EXMEM.readRegB = state.IDEX.readRegB;
        newState.EXMEM.readRegB = regB2;

        /* --------------------- MEM stage --------------------- */
        int data = state.EXMEM.aluResult;
        int code = opcode(state.EXMEM.instr);

        int aluResult = state.EXMEM.aluResult;

        if (code == SW) {
            //need to write regB into alu
            newState.dataMem[aluResult] = state.EXMEM.readRegB;
        }
        else if (code == LW) {
            //load alu into regB
            data = state.dataMem[state.EXMEM.aluResult];
        }
        else if (code == BEQ) {
            //Check if we branch
            if (state.EXMEM.eq == 1) {
                //If the ALU is equal and we are a BEQ... branch
                //Set PC to branch target, set previous three to noops
                newState.pc = state.EXMEM.branchTarget;
                newState.EXMEM.instr = NOOPINSTRUCTION;
                newState.IDEX.instr = newState.EXMEM.instr;
                newState.IFID.instr = newState.EXMEM.instr;
            }
        }

        newState.MEMWB.instr = state.EXMEM.instr;
        newState.MEMWB.writeData = data;

        /* ---------------------- WB stage --------------------- */
        newState.WBEND.instr = state.MEMWB.instr;
        newState.WBEND.writeData = state.MEMWB.writeData;
        int curOp = opcode(state.MEMWB.instr);
        if (curOp == LW) {
            newState.reg[field1(state.MEMWB.instr)] = state.MEMWB.writeData;
        }
        else if (curOp == ADD) {
            newState.reg[field2(state.MEMWB.instr)] = state.MEMWB.writeData;
        }
        else if (curOp == NOR) {
            newState.reg[field2(state.MEMWB.instr)] = state.MEMWB.writeData;
        }
        /* ------------------------ END ------------------------ */

        state = newState; /* this is the last statement before end of the loop. It marks the end
        of the cycle and updates the current state with the values calculated in this cycle */
    }
    printf("machine halted\n");
    printf("total of %d cycles executed\n", state.cycles);
    printf("final state of machine:\n");
    printState(&state);
}
void calculateRegisters(stateType* statePtr, int* regA, int* regB) {

    //Need to check for hazards in future stuff
    int newRegA = field0(statePtr->IDEX.instr);
    int newRegB = field1(statePtr->IDEX.instr);
    int writingReg = 0;
    int curALU = 0;

    //check in exmem, memwb, and wbend

    int wbEndOpcode = opcode(statePtr->WBEND.instr);
    curALU = statePtr->WBEND.writeData;

    if (wbEndOpcode == NOR || wbEndOpcode == ADD) {
        writingReg = field2(statePtr->WBEND.instr);
        if (writingReg == newRegA)
            *regA = curALU;
        if (writingReg == newRegB)
            *regB = curALU;
    }
    if (wbEndOpcode == LW) {
        writingReg = field1(statePtr->WBEND.instr);
        if (writingReg == newRegA)
            *regA = curALU;
        if (writingReg == newRegB)
            *regB = curALU;
    }
    //next state
       
    int memWbOpcode = opcode(statePtr->MEMWB.instr);
    curALU = statePtr->MEMWB.writeData;

    if (memWbOpcode == NOR || memWbOpcode == ADD) {
        writingReg = field2(statePtr->MEMWB.instr);
        if (writingReg == newRegA)
            *regA = curALU;
        if (writingReg == newRegB)
            *regB = curALU;
    }
    if (memWbOpcode == LW) {
        writingReg = field1(statePtr->MEMWB.instr);
        if (writingReg == newRegA)
            *regA = curALU;
        if (writingReg == newRegB)
            *regB = curALU;
    }

    //next test
    int exMemOpcode = opcode(statePtr->EXMEM.instr);
    curALU = statePtr->EXMEM.aluResult;

    if (exMemOpcode == NOR || exMemOpcode == ADD) {
        writingReg = field2(statePtr->EXMEM.instr);
        if (writingReg == newRegA)
            *regA = curALU;
        if (writingReg == newRegB)
            *regB = curALU;
    }
    if (exMemOpcode == LW) {
        writingReg = field1(statePtr->EXMEM.instr);
        if (writingReg == newRegA)
            *regA = curALU;
        if (writingReg == newRegB)
            *regB = curALU;
    }

    return;
}
int calculateALU(int instruction, int regA, int regB, int offset, int pc) {
    int ALU = 0;
    int opc = opcode(instruction);

    if (opc == ADD) {
        ALU = regA + regB;
    }
    else if (opc == NOR) {
        ALU = ~(regA | regB);
    }
    else if (opc == LW || opc == SW) {
        ALU = regA + offset;
    }
    else if(opc == BEQ) {
        ALU = offset + pc;
    }

    return ALU;
}
/*
* DO NOT MODIFY ANY OF THE CODE BELOW.
*/

void printInstruction(int instr) {
    char instr_opcode_str[10];
    int instr_opcode = opcode(instr);
    if(0 <= instr_opcode && instr_opcode<= NOOP) {
        strcpy(instr_opcode_str, opcode_to_str_map[instr_opcode]);
    }

    switch (instr_opcode) {
        case ADD:
        case NOR:
        case LW:
        case SW:
        case BEQ:
            printf("%s %d %d %d", instr_opcode_str, field0(instr), field1(instr), convertNum(field2(instr)));
            break;
        case JALR:
            printf("%s %d %d", instr_opcode_str, field0(instr), field1(instr));
            break;
        case HALT:
        case NOOP:
            printf("%s", instr_opcode_str);
            break;
        default:
            printf(".fill %d", instr);
            return;
    }
}

void printState(stateType *statePtr) {
    printf("\n@@@\n");
    printf("state before cycle %d starts:\n", statePtr->cycles);
    printf("\tpc = %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (int i=0; i<statePtr->numMemory; ++i) {
        printf("\t\tdataMem[ %d ] = %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (int i=0; i<NUMREGS; ++i) {
        printf("\t\treg[ %d ] = %d\n", i, statePtr->reg[i]);
    }

    // IF/ID
    printf("\tIF/ID pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IFID.instr);
    printInstruction(statePtr->IFID.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IFID.pcPlus1);
    if(opcode(statePtr->IFID.instr) == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");

    // ID/EX
    int idexOp = opcode(statePtr->IDEX.instr);
    printf("\tID/EX pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IDEX.instr);
    printInstruction(statePtr->IDEX.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IDEX.pcPlus1);
    if(idexOp == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegA = %d", statePtr->IDEX.readRegA);
    if (idexOp >= HALT || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->IDEX.readRegB);
    if(idexOp == LW || idexOp > BEQ || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\toffset = %d", statePtr->IDEX.offset);
    if (idexOp != LW && idexOp != SW && idexOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // EX/MEM
    int exmemOp = opcode(statePtr->EXMEM.instr);
    printf("\tEX/MEM pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->EXMEM.instr);
    printInstruction(statePtr->EXMEM.instr);
    printf(" )\n");
    printf("\t\tbranchTarget %d", statePtr->EXMEM.branchTarget);
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\teq ? %s", (statePtr->EXMEM.eq ? "True" : "False"));
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\taluResult = %d", statePtr->EXMEM.aluResult);
    if (exmemOp > SW || exmemOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->EXMEM.readRegB);
    if (exmemOp != SW) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // MEM/WB
	int memwbOp = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (memwbOp >= SW || memwbOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // WB/END
	int wbendOp = opcode(statePtr->WBEND.instr);
    printf("\tWB/END pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->WBEND.instr);
    printInstruction(statePtr->WBEND.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->WBEND.writeData);
    if (wbendOp >= SW || wbendOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    printf("end state\n");
    fflush(stdout);
}

// File
#define MAXLINELENGTH 1000 // MAXLINELENGTH is the max number of characters we read

void readMachineCode(stateType *state, char* filename) {
    char line[MAXLINELENGTH];
    FILE *filePtr = fopen(filename, "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", filename);
        exit(1);
    }

    printf("instruction memory:\n");
    for (state->numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; ++state->numMemory) {
        if (sscanf(line, "%d", state->instrMem+state->numMemory) != 1) {
            printf("error in reading address %d\n", state->numMemory);
            exit(1);
        }
        printf("\tinstrMem[ %d ] = ", state->numMemory);
        printInstruction(state->dataMem[state->numMemory] = state->instrMem[state->numMemory]);
        printf("\n");
    }
}
