/*****************************
 **  Computer Systems Lab I
 **        Fall 2012
 ** Lab C: Pipeline Simulator
 ** Authors: Christopher Finn
 **          Paulo Leal
 ****************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define _WIN32_WINNT 0x0500
#include <windows.h>

#define MEMSIZE 512         // # of memory lines = 2KB/32bits
#define ARRAY_SIZE 10       // Array size
#define MAX 10

///////////////
// Enums
//////////
enum OPCODE {add, addi, sub, mul, lw, sw, beq, nop, quasi};
enum REG {pc, $0, $at, $v0, $v1, $a0, $a1, $a2, $a3, $t0, $t1, $t2, $t3, $t4, $t5, $t6, $t7, $s0, $s1,
           $s2, $s3, $s4, $s5, $s6, $s7, $t8, $t9, $k0, $k1, $gp, $sp, $s8, $ra};

///////////////
// Structs
//////////
/**
 ** Instruction format
 **/
struct Instruction{
    enum OPCODE opcode;
    enum REG rd;
    enum REG rs;
    enum REG rt;
    int immediate;
};

/**
 ** Latch between ID & EX stages
 **/
typedef struct {
    int enable;
    enum OPCODE ALUCtrl;
    int regData1;
    int regData2;
    int destReg;
    int memRd;
    int memWr;
    int regWr;
    int branch;
    int quasi;
} ID_EX_REG;
ID_EX_REG ID_EX;

/**
 ** Latch between EX & MEM stages
 **/
typedef struct {
    int enable;
    int data;
    int destReg;
    int memRd;
    int memWr;
    int regWr;
    int branch;
    int quasi;
} EX_MEM_REG;
EX_MEM_REG EX_MEM;

/**
 ** Latch between MEM & WB stages
 **/
typedef struct {
    int enable;
    int data;
    int destReg;
    int regWr;
    int quasi;
}MEM_WB_REG;
MEM_WB_REG MEM_WB;

struct Queue{
    int size;
    int head;
    int tail;
    int totalSize;
    struct Instruction *instruction;
};

///////////////
// Data Structures
//////////
struct Instruction instructionMemory[512];  // 2KB Instruction Memory
int dataMemory[MEMSIZE];                    // 2KB Data Memory
int registerFile[33];                       // Holds values of all usable registers
int oldRegisterFile[33];                    // Used for highlighting changes
char labelMap[MEMSIZE][10];                 // For replacing Labels with line number
struct Queue queue;

///////////////
// Parameters
//////////
int c;  // # of cylces for memory addressing
int q;  // size of intsruction queue
int m;  // # of cycles for mul
int n;  // # of cycles for other operations
int a;  // # lower random range
int b;  // # upper random range
int numProgramLines; // number of instructions
int stepMode;        // step or batch mode
int quasiDelay;      // random delay for quasi
int graphicalMode;   // graphical or text mode

///////////////
// Flags
//////////
int branch_Pending = 0;
int ID_Stall = 0;

///////////////
// Statistical Variables
//////////
int instructionQueueAvg = 0;
int IFCycles = 0;
int IDCycles = 0;
int EXCycles = 0;
int MEMCycles = 0;
int WBCycles = 0;
int totalCycles = 0;

///////////////
// Queue Functions
//////////
/**
 ** Enqueues instruction
 **/
void enqueue(struct Instruction instr){
    queue.tail = (queue.tail + 1) % queue.totalSize;
    if(queue.tail == queue.head){
        printf("Queue Full, cannot enqueue!\n");
        return;
    }
    queue.instruction[queue.tail] = instr;
    queue.size++;
}

/**
 ** Dequeues instruction & points parameter to it
 **/
void dequeue(struct Instruction *instr){
    if(queue.head == queue.tail){
        printf("Queue Empty, cannot dequeue!\n");
        return;
    }
    queue.head = (queue.head + 1) % queue.totalSize;
    *instr = queue.instruction[queue.head];
    queue.size--;
}

/**
 ** Points parameter at head instruction but does not dequeue
 **/
void peek(struct Instruction *instr){
    if(queue.head == queue.tail){
        printf("Queue Empty, cannot dequeue!\n");
        exit(0);
    }
    int index = (queue.head + 1) % queue.totalSize;
    *instr = queue.instruction[index];
}

/**
 ** Checks if queue is empty
 **/
int isEmpty(){
    if(queue.head == queue.tail){
        return 1;
    }
    return 0;
}

/**
 ** Checks if queue is full
 **/
int isFull(){
    if(((queue.tail + 1) % queue.totalSize) == queue.head){
        return 1;
    }
    return 0;
}

/**
 ** Empties (flushes) queue
 **/
void flushQueue(){
    queue.tail = queue.head;
    queue.size = 0;
}

///////////////
// Initialization Functions
//////////
/**
 ** Parse string into OPCODE enum
 **/
void parseOpcode(char *instr, enum OPCODE *op){
    if(strncmp("addi",instr,4) == 0){
        *op = addi;
        return;
    }
    if(strncmp("add",instr,3) == 0){
        *op = add;
        return;
    }
    if(strncmp("sub",instr,3) == 0){
        *op = sub;
        return;
    }
    if(strncmp("mul",instr,3) == 0){
        *op = mul;
        return;
    }
    if(strncmp("lw",instr,2) == 0){
        *op = lw;
        return;
    }
    if(strncmp("sw",instr,2) == 0){
        *op = sw;
        return;
    }
    if(strncmp("beq",instr,3) == 0){
        *op = beq;
        return;
    }
    *op = quasi;
}

/**
 ** Parse string into REG enum
 **/
void parseReg(char *reg, enum REG *InstrReg){
    if(strncmp("$0",reg,2) == 0){
            *InstrReg = $0;
            return;
    }
    if(strncmp("$at",reg,3) == 0){
            *InstrReg = $at;
            return;
    }
    if(strncmp("$v0",reg,3) == 0){
            *InstrReg = $v0;
            return;
    }
    if(strncmp("$a0",reg,3) == 0){
            *InstrReg = $a0;
            return;
    }
    if(strncmp("$a1",reg,3) == 0){
            *InstrReg = $a1;
            return;
    }
    if(strncmp("$a2",reg,3) == 0){
            *InstrReg = $a2;
            return;
    }
    if(strncmp("$a3",reg,3) == 0){
            *InstrReg = $a3;
            return;
    }
    if(strncmp("$t0",reg,3) == 0){
            *InstrReg = $t0;
            return;
    }
    if(strncmp("$t1",reg,3) == 0){
            *InstrReg = $t1;
            return;
    }
    if(strncmp("$t2",reg,3) == 0){
            *InstrReg = $t2;
            return;
    }
    if(strncmp("$t3",reg,3) == 0){
            *InstrReg = $t3;
            return;
    }
    if(strncmp("$t4",reg,3) == 0){
            *InstrReg = $t4;
            return;
    }
    if(strncmp("$t5",reg,3) == 0){
            *InstrReg = $t5;
            return;
    }
    if(strncmp("$t6",reg,3) == 0){
            *InstrReg = $t6;
            return;
    }
    if(strncmp("$t7",reg,3) == 0){
            *InstrReg = $t7;
            return;
    }
    if(strncmp("$s0",reg,3) == 0){
            *InstrReg = $s0;
            return;
    }
    if(strncmp("$s1",reg,3) == 0){
            *InstrReg = $s1;
            return;
    }
    if(strncmp("$s2",reg,3) == 0){
            *InstrReg = $s2;
            return;
    }
    if(strncmp("$s3",reg,3) == 0){
            *InstrReg = $s3;
            return;
    }
    if(strncmp("$s4",reg,3) == 0){
            *InstrReg = $s4;
            return;
    }
    if(strncmp("$s5",reg,3) == 0){
            *InstrReg = $s5;
            return;
    }
    if(strncmp("$s6",reg,3) == 0){
            *InstrReg = $s6;
            return;
    }
    if(strncmp("$s7",reg,3) == 0){
            *InstrReg = $s7;
            return;
    }
    if(strncmp("$t8",reg,3) == 0){
            *InstrReg = $t8;
            return;
    }
    if(strncmp("$t9",reg,3) == 0){
            *InstrReg = $t9;
            return;
    }
    if(strncmp("$k0",reg,3) == 0){
            *InstrReg = $k0;
            return;
    }
    if(strncmp("$k1",reg,3) == 0){
            *InstrReg = $k1;
            return;
    }
    if(strncmp("$gp",reg,3) == 0){
            *InstrReg = $gp;
            return;
    }
    if(strncmp("$sp",reg,3) == 0){
            *InstrReg = $sp;
            return;
    }
    if(strncmp("$s8",reg,3) == 0){
            *InstrReg = $s8;
            return;
    }
    if(strncmp("$ra",reg,3) == 0){
            *InstrReg = $ra;
            return;
    }
}

/**
 ** Parse add instruction
 **/
void parseAdd(struct Instruction *instruction){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rd));
    reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rs));
    reg = strtok(NULL, "\0");
    parseReg(reg, &((*instruction).rt));
}

/**
 ** Parse sub instruction
 **/
void parseSub(struct Instruction *instruction){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rd));
    reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rs));
    reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rt));
}

/**
 ** Parse addi instruction
 **/
void parseAddi(struct Instruction *instruction){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rt));
    reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rs));
    (*instruction).immediate = atoi(strtok(NULL, "\0"));
}

/**
 ** Parse mul instruction
 **/
void parseMul(struct Instruction *instruction){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rd));
    reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rs));
    reg = strtok(NULL, "\0");
    parseReg(reg, &((*instruction).rt));
}

/**
 ** Parse lw instruction
 **/
void parseLw(struct Instruction *instruction){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rt));
    (*instruction).immediate = atoi(strtok(NULL, "("));
    reg = strtok(NULL, ")");
    parseReg(reg, &((*instruction).rs));
}

/**
 ** Parse sw instruction
 **/
void parseSw(struct Instruction *instruction){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rt));
    (*instruction).immediate = atoi(strtok(NULL, "("));
    reg = strtok(NULL, ")");
    parseReg(reg, &((*instruction).rs));
}

/**
 ** Parse beq instruction
 **/
void parseBeq(struct Instruction *instruction, int lineNum){
    char *reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rs));
    reg = strtok(NULL, ",");
    parseReg(reg, &((*instruction).rt));
    char *label = strtok(NULL, " ");
    int i;
    for(i=0;i<numProgramLines;i++){
        if(strcmp(labelMap[i],label) == 0){
            (*instruction).immediate = (i - 1) - lineNum;
            return;
        }
    }
    (*instruction).immediate = atoi(label);
}

/**
 ** Parse quasi instruction
 **/
void parseQuasi(struct Instruction *instruction){
    (*instruction).rs = 0;
    (*instruction).rt = 0;
    (*instruction).rd = 0;
}

/**
 ** Match label string to labelMap index
 **/
void parseLabels(){
    int i = 0;
    FILE *ifp;
    ifp = fopen("MatrixMultiplication.txt", "r");
    if (ifp != NULL) {
        while (!feof(ifp)) {
            char line[100];
            if(fgets(line, sizeof line, ifp)){
                char *str;
                char *label;
                str = strtok(line, "\n");   // Strip off returns
                str = strtok(str, "#");     // Strip off comments
                label = strchr(str,':');    // Look for label
                if(label != NULL){
                    strncpy(labelMap[i], str, strlen(str)-strlen(label));   // Add label
                    str = strcpy(str,label+1);                              // Remove ':'
                }
                i++;
            }
        }
        numProgramLines = i;
        fclose(ifp);
    }
    else
    {
        perror ("Failed to open file");
    }
}

/**
 ** Parse instruction lines & add to instruction memory
 **/
void instructionMemoryInit(){
    parseLabels();
    int i = 0;
    FILE *ifp;
    ifp = fopen("MatrixMultiplication.txt", "r");
    if (ifp != NULL) {
        while (!feof(ifp)) {
            char line[100];
            if(fgets(line, sizeof line, ifp)){
                struct Instruction instruction;
                char *str;
                char *label;
                str = strtok(line, "\n");   // Strip off returns
                str = strtok(str, "#");     // Strip off comments
                label = strchr(str,':');    // Look for label
                if(label != NULL){
                    str = strcpy(str,label+1);  // Remove ':'
                }
                char *instr = strtok(str, " ");
                if(strlen(str) == 0)
                    break;
                parseOpcode(instr, &(instruction.opcode));
                switch(instruction.opcode){
                    case  (add):    parseAdd(&instruction);     break;
                    case  (sub):    parseSub(&instruction);     break;
                    case (addi):    parseAddi(&instruction);    break;
                    case  (mul):    parseMul(&instruction);     break;
                    case   (lw):    parseLw(&instruction);      break;
                    case   (sw):    parseSw(&instruction);      break;
                    case  (beq):    parseBeq(&instruction, i);  break;
                    case (quasi):   parseQuasi(&instruction);   break;
                }
                instructionMemory[i] = instruction;
                i++;
            }
        }
        fclose(ifp);
    }
    else
    {
        perror ("Failed to open file");
    }
}

/**
 ** Initialize data memory
 **/
void dataMemoryInit(){
    registerFile[$t1] = 0;
    registerFile[$t2] = 100;
    registerFile[$t3] = 200;
    int i;
    for(i=0;i<ARRAY_SIZE;i++){
        int j;
        for(j=0;j<ARRAY_SIZE;j++){
            dataMemory[ARRAY_SIZE*i+j] = 2*i+j;
            dataMemory[100+ARRAY_SIZE*i+j] = 2*i+j;
        }
    }
}

/**
 ** Initialize instruction memory queue
 **/
void queueInit(struct Instruction *instructionQueue){
    queue.instruction = &instructionQueue[0];
    queue.totalSize = q;
    queue.size = 0;
    queue.head = 0;
    queue.tail = 0;
}

/**
 ** Read input parameters
 **/
void readParameters(){
    HANDLE hOut;
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |FOREGROUND_INTENSITY);
    printf("INPUT PARAMETERS \n");
    printf("----------------\n");
    printf("size of instruction queue: ");
    scanf("%d", &q);
    q = q + 1; // Compensate for the loss of 1 index due to cicular queue
    printf("# of cycles for memory addressing: ");
    scanf("%d", &c);
    printf("# of cycles for mul: ");
    scanf("%d", &m);
    printf("# of cycles for other operations: ");
    scanf("%d", &n);
    printf("random range, from: ");
    scanf("%d", &a);
    printf("\t\tto: ");
    scanf("%d", &b);
    printf("text(0) or graphical(1) mode: ");
    scanf("%d", &graphicalMode);
    printf("batch mode(0) or step mode(1): ");
    scanf("%d", &stepMode);
    printf("----------------\n");
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_INTENSITY);
}

/**
 ** Print intial screen & adjust console settings
 **/
void screenInit(){
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HWND console = GetConsoleWindow();
    RECT r;
    GetWindowRect(console, &r);
    MoveWindow(console, r.left, r.top, 425, 630, TRUE);
    SetConsoleTitle("PIPELINE SIMULATOR");
    CONSOLE_CURSOR_INFO ConCurInf;
    ConCurInf.dwSize = 10;
    ConCurInf.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ConCurInf);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_INTENSITY);                                               // Set title colour to red
    printf("  ---------------------------------------------\n");
    printf("               PIPELINE SIMULATOR\n");
    printf("         Authors: Chris Finn & Paulo Leal\n");
    printf("  ---------------------------------------------\n\n\n");
}

///////////////
// Display Functions
//////////
/**
 ** Display instruction struct
 **/
void displayStruct(struct Instruction *instr){
    switch((*instr).opcode){
         case  (add):
         case  (sub):
         case  (mul):  printf("%d %d,%d,%d\n", (*instr).opcode,(*instr).rd,(*instr).rs,(*instr).rt); break;
         case (addi):  printf("%d %d,%d,%d\n", (*instr).opcode,(*instr).rt,(*instr).rs,(*instr).immediate); break;
         case   (lw):
         case   (sw):  printf("%d %d,%d(%d)\n", (*instr).opcode,(*instr).rt,(*instr).immediate,(*instr).rs); break;
         case  (beq):  printf("%d %d,%d,%d\n", (*instr).opcode,(*instr).rs,(*instr).rt,(*instr).immediate); break;
         case (quasi): printf("Quasi Instruction\n"); break;
         default: printf("Unknown instruction\n");
    }
}

/**
 ** Display instruction memory contents
 **/
void instructionMemoryDisplay(){
    printf("----------\nInstruction Memory Display\n----------\n");
    int i;
    for(i=0;i<numProgramLines;i++){
        printf("%d:\t",i);
        displayStruct(&instructionMemory[i]);
    }
}

/**
 ** Display data memory contents
 **/
void dataMemoryDisplay(){
    printf("----------\nData Memory Display\n----------\n");
    int i;
    for(i=0;i<MEMSIZE;i++){
        printf("%d:\t%d\n", i, dataMemory[i]);
        if(dataMemory[i+1] == 0 && dataMemory [i+2] == 0)
            return;
    }
}

/**
 ** Display data memory in matrix format
 **/
void matrixDisplay(){
    printf("----------\nMatrix Display\n----------\n");
    int i;
    printf("Matrix A");
    for(i=0;i<ARRAY_SIZE;i++)
        printf("   ");
    printf("  ");
    printf("Matrix B\n");
    printf("_");
    for(i=0;i<ARRAY_SIZE;i++)
        printf("___");
    printf("\t\t");
    printf("_");
    for(i=0;i<ARRAY_SIZE;i++)
        printf("___");
    printf("\n");
    for(i=0;i<ARRAY_SIZE;i++){
        int j;
        printf("|");
        for(j=0;j<ARRAY_SIZE;j++){
            if(dataMemory[ARRAY_SIZE*i+j] < 10)
                printf(" ");
            printf("%d|",dataMemory[ARRAY_SIZE*i+j]);
        }
        printf("\t\t");
        printf("|");
        for(j=0;j<ARRAY_SIZE;j++){
            if(dataMemory[ARRAY_SIZE*i+j] < 10)
                printf(" ");
            printf("%d|",dataMemory[100+ARRAY_SIZE*i+j]);
        }
        printf("\n");
    }
    printf("\n");
    printf("Matrix C\n");
    printf("_");
    for(i=0;i<ARRAY_SIZE;i++)
        printf("_____");
    printf("\n");
    for(i=0;i<ARRAY_SIZE;i++){
        printf("|");
        int j;
        for(j=0;j<ARRAY_SIZE;j++){
            if(dataMemory[200+ARRAY_SIZE*i+j] < 10)
                printf(" ");
            if(dataMemory[200+ARRAY_SIZE*i+j] < 100)
                printf(" ");
            if(dataMemory[200+ARRAY_SIZE*i+j] < 1000)
                printf(" ");
            printf("%d|",dataMemory[200+ARRAY_SIZE*i+j]);
        }
        printf("\n");
    }
}

/**
 ** Display label map contents
 **/
void labelMapDisplay(){
    printf("----------\nLabel Map Display\n----------\n");
    int i;
    for(i=0;i<numProgramLines;i++){
        printf("%s\n", labelMap[i]);
    }
}

/**
 ** Display instruction queue contents
 **/
void instructionQueueDisplay(){
    printf("----------\nInstruction Queue Display\n----------\n");
    int i;
    printf("Head = %d, Tail = %d\n", queue.head, queue.tail);
    printf("HEAD\n");
    i = queue.head;
    int size = queue.size;
    while(i != queue.tail){
        i = (i + 1) % (queue.totalSize + 1);
        displayStruct(&queue.instruction[i]);
    }
    printf("TAIL\n");
}

/**
 ** Display output data (register values & statistics)
 **/
void outputDisplay(){
    system("cls");
    HANDLE hOut;
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_INTENSITY);                                               // Set title colour to red
    printf("  ---------------------------------------------\n");
    printf("               PIPELINE SIMULATOR\n");
    printf("        Authors: Chris Finn & Paulo Leal\n");
    printf("  ---------------------------------------------\n\n\n");
    SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_BLUE |FOREGROUND_INTENSITY);                            // Set title colour to cyan
    printf("  Registers\tStatistics\n  --------\t----------\n");
    char reg[33][4] = {"pc", "$0", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$s0",
                  "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7", "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$s8", "$ra"};
    int i;
    for(i=2;i<33;i++){
        if(registerFile[i] != oldRegisterFile[i])
            SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);                    // set changed to yellow
        else
            SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |FOREGROUND_INTENSITY);   // set unchanged to white
        printf("  %s:\t%d\t", reg[i], registerFile[i]);
        if(i == 2 && totalCycles > 0){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);                                     // set changed to green
            printf("IF cycles:\t%d [%.2f%%]", IFCycles, (100)*(double)IFCycles/totalCycles);
        }
        if(i == 3 && totalCycles > 0){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);                                     // set changed to green
            printf("ID cycles:\t%d [%.2f%%]", IDCycles, (100)*(double)IDCycles/totalCycles);
        }
        if(i == 4 && totalCycles > 0){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);                                     // set changed to green
            printf("EX cycles:\t%d [%.2f%%]", EXCycles, (100)*(double)EXCycles/totalCycles);
        }
        if(i == 5 && totalCycles > 0){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);                                     // set changed to green
            printf("MEM cycles:\t%d [%.2f%%]", MEMCycles, (100)*(double)MEMCycles/totalCycles);
        }
        if(i == 6 && totalCycles > 0){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);                                     // set changed to green
            printf("WB cycles:\t%d [%.2f%%]", WBCycles, (100)*(double)WBCycles/totalCycles);
        }
        if(i == 7){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);                                     // set changed to green
            printf("Total cycles:\t%d", totalCycles);
        }
        if(i == 9){
            SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            printf("Avg queue entries: %.2f", (double)instructionQueueAvg/totalCycles);
        }
        printf("\n");


    }
    printf("\n");
    SetConsoleTextAttribute(hOut, FOREGROUND_GREEN |FOREGROUND_INTENSITY);                                      // set pc to green
    printf("  %s:\t%d\n\n", reg[0], registerFile[0]);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |FOREGROUND_INTENSITY);   // set other text to white
}

/**
 ** Write statistics to output file
 **/
void displayStatistics(){
    FILE *fp;
    fp=fopen("OutputStatistics.txt", "a");
    fprintf(fp,"  ---------------------------------------------\n");
    fprintf(fp,"  Queue Size %d\n",q);
    fprintf(fp,"IF cycles:\t%d [%.2f%%]\n", IFCycles, (100)*(double)IFCycles/totalCycles);
    fprintf(fp,"ID cycles:\t%d [%.2f%%]\n", IDCycles, (100)*(double)IDCycles/totalCycles);
    fprintf(fp,"EX cycles:\t%d [%.2f%%]\n", EXCycles, (100)*(double)EXCycles/totalCycles);
    fprintf(fp,"MEM cycles:\t%d [%.2f%%]\n", MEMCycles, (100)*(double)MEMCycles/totalCycles);
    fprintf(fp,"WB cycles:\t%d [%.2f%%]\n", WBCycles, (100)*(double)WBCycles/totalCycles);
    fprintf(fp,"Total cycles:\t%d\n", totalCycles);
    fprintf(fp,"Avg queue entries: %.2f\n", (double)instructionQueueAvg/totalCycles);
    fprintf(fp,"\n");
    fprintf(fp,"  ---------------------------------------------\n\n");
    fclose(fp);
}

///////////////
// Utility Functions
//////////
/**
 ** Updates old register file to track changes
 **/
void updateOldRegisterFile(){
    int i;
    for(i=2;i<31;i++)
        oldRegisterFile[i] = registerFile[i];
}

/**
 ** Steps through by a cycle for every enter press
 ** 'q': quits program
 ** 'c': continues in batch mode
 **/
int checkForKeyInput(){
    int c = getchar();
    if(c == 'x')
        exit(1);
    if(c == 'c')
        stepMode = 0;
    if(c == 'q')
        instructionQueueDisplay();
    if(c == 'd')
        dataMemoryDisplay();
    if(c == 'g')
        outputDisplay();
    if(c == 'l')
        labelMapDisplay();
    return 1;
}

/**
 ** Initializes random number generator
 **/
void initRandom(){
    time_t seconds;
    time(&seconds);
    srand((unsigned int) seconds);
}

/**
 ** Return random number bewteen a & b
 **/
int calculateRandom(){
    return (rand() % (b - a + 1) + a);
}

/**
 ** Initialize latches w/ NOPS
 **/
void initLatches(){
    ID_EX.ALUCtrl = nop;
    ID_EX.enable = 1;
    EX_MEM.memRd = 0;
    EX_MEM.memWr = 0;
    EX_MEM.quasi = 0;
    EX_MEM.branch = 0;
    EX_MEM.enable = 1;
    MEM_WB.quasi = 0;
    MEM_WB.regWr = 0;
    MEM_WB.enable = 1;
}

/**
 ** Check for end conditions
 **/
int checkFinalConditions(){
    if(registerFile[pc] == (numProgramLines) && isEmpty() && ID_EX.ALUCtrl == nop && !EX_MEM.memRd
        && !EX_MEM.memWr && !EX_MEM.branch && !EX_MEM.quasi && !EX_MEM.regWr && !MEM_WB.regWr){
        return 1;
    }
    /*if(totalCycles >= 160000){
        stepMode = !stepMode;
        printf("Final Condition Check\n");
        printf("---------------------\n");
        printf("pc: %d == %d\n",registerFile[pc],numProgramLines);
        printf("isEmpty = %d\n",isEmpty());
        printf("ID_EX.ALUCtrl = %d\n", ID_EX.ALUCtrl);
        printf("EX_MEM.memRd = %d\n", EX_MEM.memRd);
        printf("EX_MEM.memWr = %d\n", EX_MEM.memWr);
        printf("EX_MEM.branch = %d\n", EX_MEM.branch);
        printf("EX_MEM.quasi = %d\n", EX_MEM.quasi);
        printf("EX_MEM.regWr = %d\n", EX_MEM.regWr);
        printf("MEM_WB.regWr = %d\n\n", MEM_WB.regWr);
    }*/
    return 0;
}

/**
 ** Check for RAW hazard
 **/
int hazardCheck(){
    int hazard = 0;
    struct Instruction instr;
    peek(&instr);
    switch(instr.opcode){
                case  (add):        if(EX_MEM.destReg == instr.rs || EX_MEM.destReg == instr.rt || MEM_WB.destReg == instr.rs || MEM_WB.destReg == instr.rt)
                                        hazard = 1;
                                    break;

                case  (sub):        if(EX_MEM.destReg == instr.rs || EX_MEM.destReg == instr.rt || MEM_WB.destReg == instr.rs || MEM_WB.destReg == instr.rt)
                                        hazard = 1;
                                    break;


                case  (mul):        if(EX_MEM.destReg == instr.rs || EX_MEM.destReg == instr.rt || MEM_WB.destReg == instr.rs || MEM_WB.destReg == instr.rt)
                                        hazard = 1;
                                    break;

                case (addi):        if(EX_MEM.destReg == instr.rs || MEM_WB.destReg == instr.rs)
                                        hazard = 1;
                                    break;

                case   (sw):        if(EX_MEM.destReg == instr.rs || EX_MEM.destReg == instr.rt || MEM_WB.destReg == instr.rs || MEM_WB.destReg == instr.rt)
                                        hazard = 1;
                                    break;

                case   (lw):        if(EX_MEM.destReg == instr.rs || MEM_WB.destReg == instr.rs)
                                        hazard = 1;
                                    break;

                case  (beq):        if(EX_MEM.destReg == instr.rs || EX_MEM.destReg == instr.rt || MEM_WB.destReg == instr.rs || MEM_WB.destReg == instr.rt)
                                        hazard = 1;
                                    break;

                    default:        break;

        }
    /*printf("HAZARD CHECK]\n");
    printf("Next Instruction: ");
    displayStruct(&instr);
    printf("rs = %d\n", instr.rs);
    printf("rt = %d\n", instr.rt);
    printf("EX_MEM.destReg = %d\n", EX_MEM.destReg);
    printf("MEM_WB.destReg = %d\n", MEM_WB.destReg);
    printf("/////////////\n");*/
    if(hazard){
        if(!graphicalMode)
            printf("ID] RAW Hazard Found: Inserting NOP\n");
        ID_EX.ALUCtrl = nop;
        ID_EX.memRd = 0;
        ID_EX.memWr = 0;
        ID_EX.quasi = 0;
        ID_EX.branch = 0;
        ID_EX.quasi = 0;
        ID_EX.regWr = 0;
        ID_EX.destReg = 0;
        return 1;
    }
    return 0;
}

///////////////
// Pipeline Stages
//////////
/**
 ** Fetch & enqueue instructions from instruction memory
 ** - delay of c
 ** - increments pc
 ** - halts on beq enqueue until resolved
 **/
void instructionFetch(int *cycles){
    if(branch_Pending == 0 && registerFile[pc] != numProgramLines && !isFull()){      // Do nothing if branch is pending, no more instructions or queue is full
        if(*cycles < c){
            IFCycles++;
            (*cycles)++;
            return;
        }
        enqueue(instructionMemory[registerFile[pc]]);                    // Enqueue next instruction
        if(!graphicalMode){
            printf("IF] Enqueued: ");
            displayStruct(&instructionMemory[registerFile[pc]]);
        }
        if(instructionMemory[registerFile[pc]].opcode == beq)            // Check for branch
            branch_Pending = 2;
        registerFile[pc]++;                                              // Increment pc by 1
        *cycles = 0;
    }
}

/**
 ** Dequeue & decode instructions
 ** - delay of 1
 ** - checks for hazards
 ** - dequeues next instruction from instruction memory queue (fetch buffer)
 ** - passes proper control signals to ID_EX latch
 ** - inserts NOPs upon hazard, empty queue and branch pending conditions
 ** - flushes queue on branch pending
 **/
void instructionDecode(){
    struct Instruction instr;
    if(branch_Pending != 1 && !isEmpty()){                                          // Insert NOP if branch is pending, queue is empty or hazard encountered
        if(ID_EX.enable){
            if(hazardCheck())                                                       // Insert NOP if hazard
                return;
            IDCycles++;
            dequeue(&instr);
            if(!graphicalMode){                                                 // Dequeue instruction -> instr
                printf("ID] Dequeued: ");
                displayStruct(&instr);
            }
            switch(instr.opcode){                                            // Parse instr & set control signals
                case  (add):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = registerFile[instr.rt];
                                    ID_EX.destReg = instr.rd;
                                    ID_EX.memRd = 0;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 1;  ID_EX.branch = 0;
                                    ID_EX.quasi = 0;
                                    break;

                case  (sub):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = registerFile[instr.rt];
                                    ID_EX.destReg = instr.rd;
                                    ID_EX.memRd = 0;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 1;  ID_EX.branch = 0;
                                    ID_EX.quasi = 0;
                                    break;


                case  (mul):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = registerFile[instr.rt];
                                    ID_EX.destReg = instr.rd;
                                    ID_EX.memRd = 0;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 1;  ID_EX.branch = 0;
                                    ID_EX.quasi = 0;
                                    break;

                case (addi):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = instr.immediate;
                                    ID_EX.destReg = instr.rt;
                                    ID_EX.memRd = 0;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 1;  ID_EX.branch = 0;
                                    ID_EX.quasi = 0;
                                    break;

                case   (sw):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = instr.immediate;
                                    ID_EX.destReg = registerFile[instr.rt];
                                    ID_EX.memRd = 0;  ID_EX.memWr = 1;
                                    ID_EX.regWr = 0;  ID_EX.branch = 0;
                                    ID_EX.quasi = 0;
                                    break;

                case   (lw):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = instr.immediate;
                                    ID_EX.destReg = instr.rt;
                                    ID_EX.memRd = 1;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 0;  ID_EX.branch = 0;
                                    ID_EX.quasi = 0;
                                    break;

                case  (beq):        ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = registerFile[instr.rs];
                                    ID_EX.regData2 = registerFile[instr.rt];
                                    ID_EX.destReg = registerFile[pc] - queue.size;
                                    ID_EX.memRd = 0;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 0;  ID_EX.branch = instr.immediate;
                                    ID_EX.quasi = 0;
                                    branch_Pending = 1;
                                    break;

              case (quasi):         ID_EX.ALUCtrl = instr.opcode;
                                    ID_EX.regData1 = 0;
                                    ID_EX.regData2 = 0;
                                    ID_EX.destReg = 0;
                                    ID_EX.memRd = 0;  ID_EX.memWr = 0;
                                    ID_EX.regWr = 0;  ID_EX.branch = 0;
                                    ID_EX.quasi = 1;
                                    break;
                   default:         break;
                }
            ID_EX.enable = 0;
        }
    }
    else
    {
        if(!isEmpty()){
            if(!graphicalMode)
                printf("ID] Flushing Instruction Queue...\n");
            flushQueue();
        }
        if(ID_EX.enable){
            if(!graphicalMode)
                printf("ID] ID STALLED, Injecting NOPS...\n");
            ID_EX.ALUCtrl = nop;
            ID_EX.memRd = 0;
            ID_EX.memWr = 0;
            ID_EX.quasi = 0;
            ID_EX.branch = 0;
            ID_EX.quasi = 0;
            ID_EX.regWr = 0;
            ID_EX.destReg = 0;
        }

    }
}

/**
 ** Execute logic & arithmatic operations
 ** - variable delay depending upon instruction type
 ** - carries out addition, subtraction, multiplication & address calculation
 ** - passes updated data along with other needed signals to EX_MEM latch
 **/
void execute(int *cycles){
    switch(ID_EX.ALUCtrl){
        case  (add):    if(*cycles < n-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            EX_MEM.data = ID_EX.regData1 + ID_EX.regData2;
                            EX_MEM.destReg = ID_EX.destReg;
                            if(!graphicalMode)
                                printf("EX] add: %d <= %d + %d\n", EX_MEM.destReg, ID_EX.regData1, ID_EX.regData2);
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                            return;
                        }
                        break;

        case  (sub):    if(*cycles < n-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            EX_MEM.data = ID_EX.regData1 - ID_EX.regData2;
                            EX_MEM.destReg = ID_EX.destReg;
                            if(!graphicalMode)
                                printf("EX] sub: %d <= %d - %d\n", EX_MEM.destReg, ID_EX.regData1, ID_EX.regData2);
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                        }
                        break;

        case (addi):    if(*cycles < n-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            EX_MEM.data = ID_EX.regData1 + ID_EX.regData2;
                            EX_MEM.destReg = ID_EX.destReg;
                            if(!graphicalMode)
                                printf("EX] addi: %d <= %d + %d\n", EX_MEM.destReg, ID_EX.regData1, ID_EX.regData2);
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                        }
                        break;

        case  (mul):    if(*cycles < m-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            EX_MEM.data = ID_EX.regData1 * ID_EX.regData2;
                            EX_MEM.destReg = ID_EX.destReg;
                            if(!graphicalMode)
                                printf("EX] addi: %d <= %d * %d\n", EX_MEM.destReg, ID_EX.regData1, ID_EX.regData2);
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                        }
                        break;

        case   (lw):    if(*cycles < n-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            EX_MEM.data = ID_EX.regData1 + (ID_EX.regData2 / 4);
                            EX_MEM.destReg = ID_EX.destReg;
                            if(!graphicalMode)
                                printf("EX] lw: %d <= dmem[%d + %d]\n", EX_MEM.destReg, ID_EX.regData1, ID_EX.regData2);
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                        }
                        break;

        case   (sw):    if(*cycles < n-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            EX_MEM.data = ID_EX.regData1 + (ID_EX.regData2 / 4);
                            EX_MEM.destReg = ID_EX.destReg;
                            if(!graphicalMode)
                                printf("EX] SW: dmem[%d + %d] <= %d\n", ID_EX.regData1, ID_EX.regData2/4, EX_MEM.destReg);
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                        }
                        break;

        case  (beq):    if(*cycles < n-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            EXCycles++;
                            if(ID_EX.regData1 == ID_EX.regData2){
                                if(!graphicalMode)
                                    printf("EX] beq: %d == %d -> branching\n", ID_EX.regData1, ID_EX.regData2);
                                EX_MEM.branch = 1;
                                EX_MEM.data = ID_EX.destReg + ID_EX.branch;
                                EX_MEM.destReg = ID_EX.destReg;
                                EX_MEM.memRd = ID_EX.memRd;
                                EX_MEM.memWr = ID_EX.memWr;
                                EX_MEM.regWr = ID_EX.regWr;
                                EX_MEM.quasi = ID_EX.quasi;
                            }else{
                                if(!graphicalMode)
                                    printf("EX] beq: %d != %d -> not branching\n", ID_EX.regData1, ID_EX.regData2);
                                EX_MEM.branch = 0;
                                branch_Pending = 0;
                                EX_MEM.destReg = 0;
                                EX_MEM.memRd = 0;
                                EX_MEM.memWr = 0;
                                EX_MEM.regWr = 0;
                                EX_MEM.quasi = 0;
                            }
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            *cycles = 0;
                        }
                        break;

        case (nop):     if(EX_MEM.enable){
                            EX_MEM.data = 0;
                            EX_MEM.destReg = ID_EX.destReg;
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            EX_MEM.enable = 0;
                        }
                        break;

        case (quasi):   if(*cycles < quasiDelay-1){
                            (*cycles)++;
                            EXCycles++;
                            return;
                        }
                        if(EX_MEM.enable){
                            if(!graphicalMode)
                                printf("EX] quasi: delay = %d cycles\n", quasiDelay);
                            EX_MEM.data = 0;
                            EX_MEM.destReg = ID_EX.destReg;
                            EX_MEM.memRd = ID_EX.memRd;
                            EX_MEM.memWr = ID_EX.memWr;
                            EX_MEM.regWr = ID_EX.regWr;
                            EX_MEM.quasi = ID_EX.quasi;
                            EX_MEM.branch = ID_EX.branch;
                            ID_EX.enable = 1;
                            EX_MEM.enable = 0;
                            quasiDelay = calculateRandom();
                            *cycles = 0;
                        }
                        break;
        }
}

/**
 ** Memory access
 ** - delay of c (except for quasi instructions which are a delay of 1)
 ** - carries out the loading and storing to & from the data memory
 ** - passes needed signals to MEM_WB latch
 **/
void memoryAccess(int *cycles){
    // Quasi
    if(EX_MEM.quasi){
        if(!MEM_WB.enable && !EX_MEM.enable){
            if(!graphicalMode){
                printf("MEM] QUASI: delaying 1 cycle\n");
                printf("MEM] WB instruction: Passing data\n");
            }
            EX_MEM.enable = 1;
            MEM_WB.enable = 1;
            MEM_WB.destReg = EX_MEM.destReg;
            MEM_WB.regWr = EX_MEM.regWr;
            MEM_WB.quasi = EX_MEM.quasi;
            MEM_WB.data = EX_MEM.data;
        }
    }

    // LW
    if(EX_MEM.memRd){
        if(*cycles < c-1){
            MEMCycles++;
            (*cycles)++;
            return;
        }
        if(!MEM_WB.enable){
            MEMCycles++;
            registerFile[EX_MEM.destReg] = dataMemory[EX_MEM.data];
            if(!graphicalMode)
                printf("MEM] LW: Loading %d into regFile[%d]\n", dataMemory[EX_MEM.data], EX_MEM.destReg);
            EX_MEM.enable = 1;
            MEM_WB.enable = 0;
            *cycles = 0;
        }
    }

    // SW
    if(EX_MEM.memWr){
        if(*cycles < c-1){
            MEMCycles++;
            (*cycles)++;
            return;
        }
        if(!MEM_WB.enable){
            MEMCycles++;
            dataMemory[EX_MEM.data] = EX_MEM.destReg;
            if(!graphicalMode)
                printf("MEM] SW: Storing %d into dmem[%d]\n", EX_MEM.destReg, EX_MEM.data);
            EX_MEM.enable = 1;
            MEM_WB.enable = 0;
            *cycles = 0;
        }
    }

    // BEQ
    if(EX_MEM.branch) {
        if(*cycles < n-1){
            MEMCycles++;
            (*cycles)++;
            return;
        }
        if(!MEM_WB.enable && !EX_MEM.enable){
            MEMCycles++;
            registerFile[pc] = EX_MEM.data;
            if(!graphicalMode)
                printf("MEM] Branch: Setting pc = %d\n", EX_MEM.data);
            branch_Pending = 0;
            EX_MEM.enable = 1;
            MEM_WB.enable = 0;
            *cycles = 0;
        }
    }

    if(!EX_MEM.quasi && !EX_MEM.memRd && !EX_MEM.memWr && !EX_MEM.branch && !EX_MEM.enable && !EX_MEM.regWr){
        EX_MEM.enable = 1;
        MEM_WB.enable = 1;
        MEM_WB.destReg = EX_MEM.destReg;
        MEM_WB.regWr = EX_MEM.regWr;
        MEM_WB.quasi = EX_MEM.quasi;
        MEM_WB.data = EX_MEM.data;
    }

    if(!EX_MEM.enable){
        if(!graphicalMode)
            printf("MEM] WB instruction: Passing data\n");
        MEM_WB.enable = 1;
        MEM_WB.destReg = EX_MEM.destReg;
        MEM_WB.regWr = EX_MEM.regWr;
        MEM_WB.quasi = EX_MEM.quasi;
        MEM_WB.data = EX_MEM.data;
    }
}

/**
 ** Writes data to register file
 ** - delay of 1
 ** - writes data back to destination register
 **/
void writeBack(){
    if(MEM_WB.quasi && MEM_WB.enable){
        if(!graphicalMode)
            printf("WB] Quasi instruction: delaying 1 cycle\n");
        MEM_WB.enable = 0;
    }
    if(MEM_WB.regWr && MEM_WB.enable)
    {
        WBCycles++;
        if(!graphicalMode)
            printf("WB] Writing %d to register #%d\n", MEM_WB.data, MEM_WB.destReg);
        registerFile[MEM_WB.destReg] = MEM_WB.data;
        EX_MEM.enable = 1;
        MEM_WB.enable = 0;
    }
    if(!MEM_WB.quasi && !MEM_WB.regWr && MEM_WB.enable){
        EX_MEM.enable = 0;
        MEM_WB.enable = 0;
    }
}

///////////////
// Driver Methods
//////////
/**
 ** Pipeline driving function
 ** - Controls cycle-by-cycle simulation of the pipeline
 ** - Calls all five stages in reverse order
 ** - Updates display (step mode only)
 ** - Checks final condition
 **/
void run(){
    initLatches();
    initRandom();
    int IF_count = 0, EX_count = 0, MEM_count = 0;
    quasiDelay = calculateRandom();
    while(!stepMode || checkForKeyInput() == 1)
    {
        if(!graphicalMode)
            printf("\ncycle #%d\n---\n", totalCycles);
        //else
          //  outputDisplay();
        ///////////////
        // Pipeline
        //////////
        writeBack();
        memoryAccess(&MEM_count);
        execute(&EX_count);
        instructionDecode();
        instructionFetch(&IF_count);

        ///////////////
        // Update Stats
        //////////
        totalCycles++;
        instructionQueueAvg += queue.size;
        //outputDisplay();
        if(stepMode && graphicalMode){
            outputDisplay();
            updateOldRegisterFile();
        }

        ///////////////
        // Check End Conditions
        //////////
        if(checkFinalConditions())
            return;
    }
}

/**
 ** Main method
 ** - calls initializing methods
 ** - calls run()
 ** - updates final display
 **/
int main(){
    screenInit();
    readParameters();
    struct Instruction instructionQueue[q];
    instructionMemoryInit();
    registerFile[pc]=0;
    dataMemoryInit();
    queueInit(instructionQueue);
    run();
    outputDisplay();
    // Uncomment to see Matrix output
    matrixDisplay();
    displayStatistics();
}





