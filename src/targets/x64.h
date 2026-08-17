#ifndef TARGET_X64
#define TARGET_X64

#include "../system.h"
#include "chasm/asm_x64.h"

typedef enum X64_Reg : u16 {
    X64_REG_RAX,
    X64_REG_RBX,
    X64_REG_RCX,
    X64_REG_RDX,
    X64_REG_RSI,
    X64_REG_RDI,
    X64_REG_RBP,
    X64_REG_RSP,
    X64_REG_R8,
    X64_REG_R9,
    X64_REG_R10,
    X64_REG_R11,
    X64_REG_R12,
    X64_REG_R13,
    X64_REG_R14,
    X64_REG_R15,

    X64_REG__COUNT,
} X64_Reg;

// void x64_translate(Vec(UOp) uops, Vec(u8)* code_out);
void x64_translate(Compiler* c);

#endif // TARGET_X64
