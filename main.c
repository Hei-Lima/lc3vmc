/* I know this type of commenting is quite lazy, but i'm using it to learn. */

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
/* unix only */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

/* Max Size of Memory */
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];  /* 65536 locations */

/* PCOFFSET MACROS */
#define PCOFFSET9 sign_extend(instr & 0x1FF, 9)
#define PCOFFSET11 sign_extend(instr & 0x7FF, 11)

/* Register Enum */
enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter */
    R_COND,
    R_COUNT
};
uint16_t reg[R_COUNT]; /* Register array */

/* Operations Enum */
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

/* Contitional Register Enum */
enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

/* Utils */

uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

int chk_input(int argc, const char* argv[]) {

    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    /* Help */
    if (strcmp(argv[2], "--HELP") == 0|| strcmp(argv[2], "--help" == 0)) 
    {
        printf("How to use: \nlc3 [image-file1] ...\n");
        exit(0);
    } 

    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

int main(int argc, const char* argv[])
{
    chk_input(argc, argv);

    @{Load Arguments}
    @{Setup}

    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;

    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
            case OP_ADD:
                {
                    /* destination register (DR) */
                    uint16_t r0 = (instr >> 9) & 0x7;
                    /* first operand (SR1) */
                    uint16_t r1 = (instr >> 6) & 0x7;
                    /* whether we are in immediate mode */
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] + imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] + reg[r2];
                    }

                    update_flags(r0);
                }
                break;
            case OP_AND:
                {
                    /* Destination register */
                    uint16_t r0 = (instr >> 9) & 0x7;
                    /* First source */
                    uint16_t r1 = (instr >> 6) & 0x7;
                    /* Check flag */
                    uint16_t imm_flag = (instr >> 5) & 0x1;

                    if (imm_flag)
                    {
                        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                        reg[r0] = reg[r1] & imm5;
                    }
                    else
                    {
                        uint16_t r2 = instr & 0x7;
                        reg[r0] = reg[r1] + reg[r2]l
                    }
                    
                    update_flags(r0);
                }
                break;
            case OP_NOT:
                {   /* Soruce (i could name it source, actually.) */
                    uint16_t r0 = (instr >> 9) & 0x7;
                    /* Destination Register */
                    uint16_t r1 = (instr >> 6) & 0x7;
                    
                    reg[r0] = ~reg[r1];

                    update_flags(r0);
                }
                break;
            case OP_BR:
                {
                    uint16_t pc_offset = PCOFFSET11;

                    uint16_t n = (instr >> 11) & 0x1;
                    uint16_t z = (instr >> 10) & 0x1;
                    uint16_t p = (instr >> 9) & 0x1;

                    if ((n && (reg[R_COND] & FL_NEG)) || 
                        (z && (reg[R_COND] & FL_ZRO)) || 
                        (p && (reg[R_COND] & FL_POS))) 
                    {
                        reg[R_PC] = reg[R_PC] + pc_offset;
                    }
                }
                break;
            case OP_JMP:
                {
                    /* Jump location instr[8:6] */
                    uint16_t jl = (instr >> 6) & 0x1;
                    reg[R_PC] = reg[jl];
                }
                break;
            case OP_JSR:
                {
                    reg[R_R7] = reg[R_PC];
                    u_int16_t flg = (instr >> 11) & 0x1;
                    /* If it is JSRR (instr[11] == 0 (Flag)): */
                    if (flg == 0) {
                        u_int16_t baseR = (instr >> 6) & 0x7;
                        reg[R_PC] = reg[baseR];
                    }
                    else
                    {
                        /* If it is JSR (instr[11] == 1 (Flag)): */
                        u_int16_t pc_offset = PCOFFSET11;
                        reg[R_PC] = reg[R_PC] + pc_offset;
                    }
                }   
                break;
            case OP_LD:
                {
                    u_int16_t r1 = (instr >> 9) & 0x7; 
                    u_int16_t pc_offset = PCOFFSET9;

                    reg[r1] = mem_read(reg[R_PC] + pc_offset);
                    update_flags(r1); 
                }
                break;
            case OP_LDI:
                {
                    /* destination register (DR) */
                    uint16_t r0 = (instr >> 9) & 0x7;
                    /* PCoffset 9*/
                    uint16_t pc_offset = PCOFFSET9;
                    /* add pc_offset to the current PC, look at that memory location to get the final address */
                    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                    update_flags(r0);
                }
                break;
            case OP_LDR:
                {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t r1 = (instr >> 6) & 0x7;
                uint16_t offset = sign_extend(instr & 0x3F, 6);
                reg[r0] = mem_read(reg[r1] + offset);
                update_flags(r0);
                }
                break;
            case OP_LEA:
                {
                    uint16_t pc_offset = PCOFFSET9;
                    uint16_t r0 = (instr >> 9) & 0x7;
                    reg[r0] = reg[R_PC] + pc_offset;
                    update_flags(r0);
                }
                break;
            case OP_ST:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = PCOFFSET9;
                    mem_write(reg[R_PC] + pc_offset, reg[r0]);
                }
                break;
            case OP_STI:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
                }
                break;
            case OP_STR:
                {
                    uint16_t r0 = (instr >> 9) & 0x7;
                    uint16_t r1 = (instr >> 6) & 0x7;
                    uint16_t offset = sign_extend(instr & 0x3F, 6);
                    mem_write(reg[r1] + offset, reg[r0]);
                }
                break;
            case OP_TRAP:
                {}
                break;
            case OP_RES:
            case OP_RTI:
            default:
                @{BAD OPCODE}
                break;
        }
    }
    @{Shutdown}
}

int chk_input(int argc, const char* argv[]) {

    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    /* Help */
    if (strcmp(argv[2], "--HELP") == 0|| strcmp(argv[2], "--help" == 0)) 
    {
        printf("How to use: \nlc3 [image-file1] ...\n");
        exit(0);
    } 

    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

