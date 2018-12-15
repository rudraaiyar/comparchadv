#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }
    
    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);
       // printf("instr \t %d\n", instr);


        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

	UpdatePC(&d,val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    //printf("fetching boi: %d\n", addr);

    return mips.memory[(addr-0x00400000)/4];
}

/*
    addu Rdest, Rsrc1, Rsrc2 : 0x00
    addiu Rdest, Rsrc1, imm : 0x09
    subu Rdest, Rsrc1, Rsrc2: 0x00
    sll Rdest, Rsrc, shamt : 0x00
    srl Rdest, Rsrc, shamt : 0x00
    and Rdest, Rsrc1, Rsrc2 : 0x00
    andi Rdest, Rsrc, imm : 0X0C = 12
    or Rdest, Rsrc1, Rsrc2 : 0x00
    ori Rdest, Rsrc, imm : 0X0D = 13
    lui Rdest, imm : 0x0F = 15
    slt Rdest, Rsrc1, Rsrc2 : 0x00
    beq Rsrc1, Rsrc2, raddr : 0x04
    bne Rsrc1, Rsrc2, raddr : 0x05
    j address : 0x02
    jal address : 0x03
    jr Rsrc : 0x00
    lw Rdest, offset: 0x23 =35
    sw Rsrc, offset :x0x2B = 43
 */

//https://en.wikibooks.org/wiki/MIPS_Assembly/Instruction_Formats#R_Instructions
//mips sheet: https://inst.eecs.berkeley.edu/~cs61c/resources/MIPS_Green_Sheet.pdf
//j instruction: https://www.d.umn.edu/~gshute/mips/jtype.xhtml
/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */


    //printf("instr \t %d\n", instr);

    
    d -> op = instr >> 26;
    if (d -> op == 0) { //r type via opcode
        d -> type = R;
        
        unsigned int rs =instr <<6;
        rs = rs >>27;
        d -> regs.r.rs =rs;
        rVals -> R_rs =mips.registers[rs];
       // printf("rs \t %d\n", rs);
        
        unsigned int rt = instr<<11;
        rt =rt >>27;
        d -> regs.r.rt =rt;
        rVals -> R_rt =mips.registers[rt];
        //printf("rt\t %d\n", rt);
        
        unsigned int rd = instr<<16;
        rd =rd >>27;
        d -> regs.r.rd =rd;
        rVals-> R_rd =mips.registers[rd];
        //printf("rd \t %d\n", rd);
        
        unsigned int shamt = instr<<21;
        shamt =shamt >>27;
        d -> regs.r.shamt =shamt;
        
        unsigned int funct = instr<<26;
        funct = funct >>26;
        d -> regs.r.funct=funct;
    }else{
        if(d -> op ==3 || d -> op ==2){ //j type via opcode
            d-> type =J;
            unsigned int target = instr<<6;
            target =target >>4;
            d -> regs.j.target =target;
            
        }else{ //i type via opcode
            d -> type =I;
            
            unsigned int rs =instr<<6;
            rs =rs >>27;
            d-> regs.i.rs =rs;
            rVals -> R_rs = mips.registers[rs];
            
            unsigned int rt = instr <<11;
            rt =rt>>27;
            d -> regs.i.rt=rt;
            rVals ->R_rt= mips.registers[rt];
            
            unsigned int unsignedImmd;
            int signedImmd;
            
            switch(d->op){
                case 4:
                    //unsignedImmd
                    unsignedImmd =instr<<16;
                    unsignedImmd =unsignedImmd>>16;
                    unsignedImmd =(unsignedImmd *4) +4 +mips.pc;
                    d -> regs.i.addr_or_immed =unsignedImmd;
                    break;
                    
                case 5:
                    //unsignedImmd
                    unsignedImmd =instr<<16;
                    unsignedImmd =unsignedImmd>>16;
                    unsignedImmd =(unsignedImmd *4) +4 +mips.pc;
                    d -> regs.i.addr_or_immed =unsignedImmd;
                    break;

                case 9:
                    //signedImmd
                    signedImmd = instr <<16;
                    signedImmd = signedImmd >> 16;
                    d -> regs.i.addr_or_immed = signedImmd;
                    break;

                case 12:
                    //unsignedImmd
                    unsignedImmd =instr <<16;
                    unsignedImmd =unsignedImmd >>16;
                    d -> regs.i.addr_or_immed =unsignedImmd;
                    break;
                    
                case 13:
                    //unsignedImmd
                    unsignedImmd =instr <<16;
                    unsignedImmd =unsignedImmd >>16;
                    d -> regs.i.addr_or_immed =unsignedImmd;
                    break;
                    
                case 15:
                    //signedImmd
                    signedImmd = instr <<16;
                    signedImmd = signedImmd >> 16;
                    d -> regs.i.addr_or_immed = signedImmd;
                    break;

                case 35:
                    //signedImmd
                    signedImmd = instr <<16;
                    signedImmd = signedImmd >> 16;
                    d -> regs.i.addr_or_immed = signedImmd;
                    break;

                case 43:
                    //signedImmd
                    signedImmd = instr <<16;
                    signedImmd = signedImmd >> 16;
                    d -> regs.i.addr_or_immed = signedImmd;
                    break;
            }
        }
    }
}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    /* Your code goes here */
    int rrs = d->regs.r.rs;
    int rrt = d->regs.r.rt;
    int rd = d->regs.r.rd;
    
    int target = d->regs.j.target;

    int irs = d->regs.i.rs;
    int irt = d->regs.i.rt;
    int imm = d->regs.i.addr_or_immed;
    
    switch (d -> op) {
        case 0:
            //r instruction based of function since all opcodes are 0
            
            /* funct decimal
             addu 33
             subu 35
             sll 0
             srl 2
             and 36
             or 37
             slt 42
             jr 8
             */
            
            switch(d -> regs.r.funct) {
                case 0:
                    printf("sll\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
                case 2:
                    printf("srl\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
                case 8:
                    printf("jr\t$%d\n", rrs);
                    break;
                case 33 :
                    printf("addu\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
                case 35 :
                    printf("subu\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
                case 36:
                    printf("and\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
                case 37:
                    printf("or\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
                case 42:
                    printf("slt\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    break;
               
            }
            break;
            
        case 2:
            //j instruction
            printf("j\t0x%08x\n", target);
            break;
        case 3:
            //j instruction
            printf("jal\t0x%08x\n", target);
            break;
        case 4:
            //i instruction
            printf("beq\t$%d, $%d, 0x%08x\n", irs, irt, imm);
            break;
        case 5:
            //i instruction
            printf("bne\t$%d, $%d, 0x%08x\n", irs, irt, imm);
            break;
        case 9:
            //i instruction
            printf("addiu\t$%d, $%d, %d\n", irt, irs, imm);
            break;
        case 12:
            //i instruction
            printf("andi\t$%d, $%d, 0x%x\n", irt, irs, imm);
            break;
        case 13:
            //i instruction
            printf("ori\t$%d, $%d, 0x%x\n", irt, irs, imm);
            break;
        case 15:
            //i instruction
            printf("lui\t$%d, 0x$%x\n", irs, imm);
            break;
        case 35:
            //i instruction
            printf("lw\t$%d, %d($%d)\n", irt, imm, irs);
            break;
        case 43:
            //i instruction
            printf("sw\t$%d, %d($%d)\n", irt, imm, irs);
            break;

    }
    
    /* funct decimal
     addu 33
     subu 35
     sll 0
     srl 2
     and 36
     or 37
     slt 42
     jr 8
     */
    if (d-> op == 0 || d-> op == 2 || d-> op == 3 || d-> op == 4 || d-> op == 5 || d-> op == 9 || d-> op == 12 || d-> op == 13 || d-> op == 15 || d-> op == 35 || d-> op == 43) {
       /* if(d->regs.r.funct == 33 || d->regs.r.funct == 35 || d->regs.r.funct == 0 || d->regs.r.funct == 2 || d->regs.r.funct == 36 || d->regs.r.funct == 37 || d->regs.r.funct == 42 || d->regs.r.funct == 8){
            
        }*/
        
        //supported instr
    }else{
        exit(0);
    }
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */
    unsigned int rrs = rVals->R_rs;
    unsigned int rrt = rVals->R_rt;
    unsigned int rd = rVals -> R_rd;
    
   // int target = d->regs.j.target;
    
    int irs = rVals->R_rs;
    int irt = rVals-> R_rt;
    int imm = d->regs.i.addr_or_immed;
    
    int result =0;
    
    switch (d -> op) {
        case 0:
            //r instruction based of function since all opcodes are 0
            
            /* funct decimal
             addu 33
             subu 35
             sll 0
             srl 2
             and 36
             or 37
             slt 42
             jr 8
             */
            
            switch(d->regs.r.funct) {
                case 0:
                    //printf("sll\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    rd = rrt << d-> regs.r.shamt;
                    break;
                case 2:
                    //printf("srl\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    rd = rrt >> d->regs.r.shamt;
                    break;
                case 8:
                    //printf("jr\t$%d\n", rrs);
                    return rrs;
                case 33 :
                    //printf("addu\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    rd = rrs + rrt;
                    break;
                case 35 :
                    //printf("subu\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    rd = rrs - rrt;
                    break;
                case 36:
                    //printf("and\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    rd = rrs & rrt;
                    break;
                case 37:
                    //printf("or\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    rd = rrs | rrt;
                    break;
                case 42:
                    //printf("slt\t$%d, $%d, $%d\n", rd, rrs, rrt);
                    if (rrs < rrt) {
                        rd = 1;
                    }else {
                        rd = 0;
                    }
                    break;
            }
            rVals->R_rd = rd;
            return rd;
        case 2:
            //j instruction
            //printf("j\t0x%08x\n", target);
            return 0;
            break;
        case 3:
            //j instruction
            //printf("jal\t0x%08x\n", target);
            return (mips.pc + 4);
            break;
        case 4:
            //i instruction
            //printf("beq\t$%d, $%d, 0x%08x\n", irs, irt, imm);
            result = irs - irt;
            break;
        case 5:
            //i instruction
            //printf("bne\t$%d, $%d, 0x%08x\n", irs, irt, imm);
            result = irs - irt;
            break;
        case 9:
            //i instruction
            //printf("addiu\t$%d, $%d, %d\n", irt, irs, imm);
            irt = imm + irs;
            result = irt;
            break;
        case 12:
            //i instruction
            //printf("andi\t$%d, $%d, 0x%x\n", irt, irs, imm);
            irt= irs & imm;
            result = irt;
            break;
        case 13:
            //i instruction
            //printf("ori\t$%d, $%d, 0x%x\n", irt, irs, imm);
            irt = irs| imm;
            result = irt;
            break;
        case 15:
            //i instruction
            //printf("lui\t$%d, 0x$%x\n", irs, imm);
            irt = imm << 16;
            result = irt;
            break;
        case 35:
            //i instruction
            //printf("lw\t$%d, %d($%d)\n", irt, imm, irs);
            irt = irs + imm;
            result = irt;
            //printf("lw reg: %d\n", result);
            break;
        case 43:
            //i instruction
            //printf("sw\t$%d, %d($%d)\n", irt, imm, irs);
            result = irs + imm;
            //printf("sw reg: %d\n", result);
            break;
            
    }
  return result;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    /* Your code goes here */
    //val is current val of mips.pc
    /*
     instructions that have branches and jumps
        bne, j, jal, beq, jr (r type)
     */
    
    int target = d -> regs.j.target;
    int imm = d -> regs.i.addr_or_immed;
    int funct = d -> regs.r.funct;
    
    switch (d -> op) {
        case 0: //jr  PC=R[rs]
            if(funct == 8){
                mips.pc = val;
            }else{
                mips.pc+=4;
            }
            break;
        case 2: //j PC=JumpAddr
            mips.pc = target;
            break;
        case 3: //jal PC=JumpAddr
            mips.pc = target;
            break;
        case 4: //beq  PC=PC+4+BranchAddr (add pc to imm value)
            if (val == 0) {
                mips.pc = imm;
            }else{
                mips.pc+=4;
            }
            break;
        case 5: //bne  PC=PC+4+BranchAddr
            if (val != 0) {
                mips.pc = imm;
            }else{
                mips.pc+=4;
            }
            break;
        default:
            mips.pc+=4;
    }
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    /* Your code goes here */
    
    //4194304 decimal to 0x00400000
    //4210687 decimal to 0x00403fff

    if (d -> op != 43 && d -> op != 35) {
        *changedMem =-1;
        return val;
        
    }else{
    
        //sw :update value in memory so we gotta update and then store the correct one in a register
        // so we dont want to update the registers just update the memory
        //add to increase location of memory
    
        //lw: doesnt store anything so we dont update anything just gets value from memory
    
        //printf("Val : %d\n", val);

        if (val < 4194304 || val > 4210687 || val % 4 != 0) { //outta bounds
            printf("Memory Access Exception at 0x%.8x: address 0x%.8x\n", mips.pc - 4, val);
            exit(0);
        }else{
            *changedMem =-1;

            switch (d -> op) {
                case 35: //lw R[rt] = M[R[rs]+SignExtImm]
                    //printf("fetching boi: %d\n", Fetch(val));
                    
                    //sum of base + addr => index
                    return Fetch(val);
                
                case 43: //sw M[R[rs]+SignExtImm] = R[rt]
                    *changedMem =val;
                    //printf("changed val boi: %d\n", val);

                    mips.memory[(val - 4194304)/4] = mips.registers [d-> regs.i.rt]; //i type -> rt
                    return 0;

                default:
                    return val;
            }
        }
    }
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    /* Your code goes here */
    
    /* funct decimal
     addu 33
     subu 35
     sll 0
     srl 2
     and 36
     or 37
     slt 42
     jr 8
     */
    
    *changedReg = -1;

    if(d->type == R){
        //r types, that aren't jumps
        if(d->regs.r.funct == 33  || d->regs.r.funct == 0 ||
           d->regs.r.funct == 2 || d->regs.r.funct == 36 || d->regs.r.funct == 37 ||
           d->regs.r.funct == 42){
            *changedReg = d->regs.r.rd;
            mips.registers[*changedReg] = val;
            return;
        }
        //jr
        *changedReg = -1;
        return;

    }


    if(d->type == I){
        //i types
        if(d->op == 9 || d->op == 12 || d->op == 13 ||
           d->op == 15 || d-> op== 35){
            *changedReg = d->regs.i.rt;
            mips.registers[*changedReg] = val;
            return;
        }
        //beq, bne, or sw
        *changedReg = -1;
        return;
    }
    
    if(d->type == J){
        //j types, this would be jal
        if(d->op == 3){
            *changedReg = 31;
            mips.registers[*changedReg] = val;
            return;
        }
        //j
        *changedReg = -1;
        return;
    }
}
