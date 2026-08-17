#ifndef ASM_VARIANTS_H
#define ASM_VARIANTS_H

#include "common/str.h"
#include "../aphelion.h"

typedef enum: u8 {
    ASM_OPERAND_NONE = 0,
    ASM_OPERAND_GPR,
    ASM_OPERAND_CTRL,
    ASM_OPERAND_MEM,
    ASM_OPERAND_IMM,

    // contrained kinds
    ASM_OPERAND_MEM_8,
    ASM_OPERAND_MEM_16,
    ASM_OPERAND_MEM_32,
    ASM_OPERAND_MEM_64,
    ASM_OPERAND_MEM_SM_8,
    ASM_OPERAND_MEM_SM_16,
    ASM_OPERAND_MEM_SM_32,
    ASM_OPERAND_MEM_SM_64,
    ASM_OPERAND_I9,
    ASM_OPERAND_U9,
    ASM_OPERAND_I14,
    ASM_OPERAND_U14,
    ASM_OPERAND_I19,
    ASM_OPERAND_U19,

    ASM_OPERAND_U6,
    ASM_OPERAND_U16,
    ASM_OPERAND_SSI_SHIFT,
} AsmOperandKind;

typedef struct AsmOperandMem {
    AphelGpr r1;
    AphelGpr r2;
    u32 i;
} AsmOperandMem;

typedef struct AsmOperand {
    AsmOperandKind kind;
    union {
        AphelGpr gpr;
        AphelCtrl ctrl;
        i32 imm;
        AsmOperandMem mem;
    };
} AsmOperand;

typedef struct AsmTemplate {
    const char* name;
    AsmOperandKind operands[4];
} AsmTemplate;

typedef struct AsmParsedInst {
    string name;
    AsmOperand operands[4];
} AsmParsedInst;

u32 encode_inst(string line);

// (name, args, expr)
// (expr) is evaluated as if it has access to the operands through o1, o2, o3, and o4
#define INST_VARIANTS \
    VARIANT( "lw"  , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_64                     }) , asm_encode_c(OP_LW , o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i / 8) ) \
    VARIANT( "lh"  , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_32                     }) , asm_encode_c(OP_LH , o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i / 4) ) \
    VARIANT( "lq"  , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_16                     }) , asm_encode_c(OP_LQ , o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i / 2) ) \
    VARIANT( "lb"  , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_8                      }) , asm_encode_c(OP_LB , o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i    ) ) \
    VARIANT( "llw" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_64                     }) , asm_encode_c(OP_LLW, o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i / 8) ) \
    VARIANT( "llh" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_32                     }) , asm_encode_c(OP_LLH, o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i / 4) ) \
    VARIANT( "llq" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_16                     }) , asm_encode_c(OP_LLQ, o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i / 2) ) \
    VARIANT( "llb" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_8                      }) , asm_encode_c(OP_LLB, o1.gpr, o2.mem.r1, o2.mem.r2, o2.mem.i    ) ) \
    VARIANT( "sw"  , G({ ASM_OPERAND_MEM_64, ASM_OPERAND_GPR                        }) , asm_encode_c(OP_SW , o2.gpr, o1.mem.r1, o1.mem.r2, o1.mem.i / 8) ) \
    VARIANT( "sh"  , G({ ASM_OPERAND_MEM_32, ASM_OPERAND_GPR                        }) , asm_encode_c(OP_SH , o2.gpr, o1.mem.r1, o1.mem.r2, o1.mem.i / 4) ) \
    VARIANT( "sq"  , G({ ASM_OPERAND_MEM_16, ASM_OPERAND_GPR                        }) , asm_encode_c(OP_SQ , o2.gpr, o1.mem.r1, o1.mem.r2, o1.mem.i / 2) ) \
    VARIANT( "sb"  , G({ ASM_OPERAND_MEM_8 , ASM_OPERAND_GPR                        }) , asm_encode_c(OP_SB , o2.gpr, o1.mem.r1, o1.mem.r2, o1.mem.i    ) ) \
    VARIANT( "scw" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_SM_64, ASM_OPERAND_GPR }) , asm_encode_c(OP_SCW, o3.gpr, o1.gpr   , o2.mem.r1, o2.mem.i / 8) ) \
    VARIANT( "sch" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_SM_32, ASM_OPERAND_GPR }) , asm_encode_c(OP_SCH, o3.gpr, o1.gpr   , o2.mem.r1, o2.mem.i / 4) ) \
    VARIANT( "scq" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_SM_16, ASM_OPERAND_GPR }) , asm_encode_c(OP_SCQ, o3.gpr, o1.gpr   , o2.mem.r1, o2.mem.i / 2) ) \
    VARIANT( "scb" , G({ ASM_OPERAND_GPR   , ASM_OPERAND_MEM_SM_8 , ASM_OPERAND_GPR }) , asm_encode_c(OP_SCB, o3.gpr, o1.gpr   , o2.mem.r1, o2.mem.i    ) ) \
    \
    VARIANT( "fence"          , G({                 }) , asm_encode_a(OP_FENCE , GPR_ZR,   0b11) ) \
    VARIANT( "fence.s"        , G({                 }) , asm_encode_a(OP_FENCE , GPR_ZR,   0b10) ) \
    VARIANT( "fence.l"        , G({                 }) , asm_encode_a(OP_FENCE , GPR_ZR,   0b01) ) \
    VARIANT( "cinval.block"   , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CINVAL, o1.gpr, 0b0011) ) \
    VARIANT( "cinval.page"    , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CINVAL, o1.gpr, 0b0111) ) \
    VARIANT( "cinval.all"     , G({                 }) , asm_encode_a(OP_CINVAL, GPR_ZR, 0b1011) ) \
    VARIANT( "cinval.i.block" , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CINVAL, o1.gpr, 0b0010) ) \
    VARIANT( "cinval.i.page"  , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CINVAL, o1.gpr, 0b0110) ) \
    VARIANT( "cinval.i.all"   , G({                 }) , asm_encode_a(OP_CINVAL, GPR_ZR, 0b1010) ) \
    VARIANT( "cinval.d.block" , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CINVAL, o1.gpr, 0b0001) ) \
    VARIANT( "cinval.d.page"  , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CINVAL, o1.gpr, 0b0101) ) \
    VARIANT( "cinval.d.all"   , G({                 }) , asm_encode_a(OP_CINVAL, GPR_ZR, 0b1001) ) \
    VARIANT( "cfetch.l"       , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b001) ) \
    VARIANT( "cfetch.s"       , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b010) ) \
    VARIANT( "cfetch.i"       , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b100) ) \
    VARIANT( "cfetch.ls"      , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b011) ) \
    VARIANT( "cfetch.li"      , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b101) ) \
    VARIANT( "cfetch.si"      , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b110) ) \
    VARIANT( "cfetch.lsi"     , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_CFETCH, o1.gpr,  0b111) ) \
    VARIANT( "tinval.page"    , G({ ASM_OPERAND_GPR }) , asm_encode_a(OP_TINVAL, o1.gpr,   0b00) ) \
    VARIANT( "tinval.priv"    , G({                 }) , asm_encode_a(OP_TINVAL, o1.gpr,   0b01) ) \
    VARIANT( "tinval.all"     , G({                 }) , asm_encode_a(OP_TINVAL, o1.gpr,   0b10) ) \
    \
    VARIANT( "ssi"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_U16, ASM_OPERAND_SSI_SHIFT }) , asm_encode_a(OP_SSI, o1.gpr,     ((o3.imm / 16) << 1) | (o2.imm << 3)) ) \
    VARIANT( "ssi.c" , G({ ASM_OPERAND_GPR, ASM_OPERAND_U16, ASM_OPERAND_SSI_SHIFT }) , asm_encode_a(OP_SSI, o1.gpr, 1 | ((o3.imm / 16) << 1) | (o2.imm << 3)) ) \
    \
    VARIANT( "add"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_ADD  , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "add"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_ADD  , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "sub"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SUB  , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "sub"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_SUB  , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "mul"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_MUL  , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "mul"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I9 }) , asm_encode_c(OP_MUL  , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "umulh" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_UMULH, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "umulh" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_UMULH, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "imulh" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_IMULH, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "imulh" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_IMULH, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "udiv"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_UDIV , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "udiv"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_UDIV , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "idiv"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_IDIV , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "idiv"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I9 }) , asm_encode_c(OP_IDIV , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "urem"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_UREM , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "urem"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_UREM , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "irem"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_IREM , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "irem"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I9 }) , asm_encode_c(OP_IREM , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    \
    VARIANT( "addi"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_ADDI , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "subi"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_SUBI , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "muli"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I14 }) , asm_encode_b(OP_MULI , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "udivi" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_UDIVI, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "idivi" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I14 }) , asm_encode_b(OP_IDIVI, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "uremi" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_UREMI, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "iremi" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I14 }) , asm_encode_b(OP_IREMI, o1.gpr, o2.gpr, o3.imm) ) \
    \
    VARIANT( "and" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_AND, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "and" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_AND, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "or"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_OR , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "or"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_OR , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "nor" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR,                }) , asm_encode_c(OP_NOR, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "nor" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_NOR, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "xor" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_XOR, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "xor" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_XOR, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    \
    VARIANT( "andi" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_ADDI , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "ori"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_ORI  , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "nori" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_NORI , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "xori" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_XORI , o1.gpr, o2.gpr, o3.imm) ) \
    \
    VARIANT( "sl"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 }) , asm_encode_c(OP_SL , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "sl"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6                  }) , asm_encode_c(OP_SL , o1.gpr, o2.gpr, GPR_ZR, o3.imm) ) \
    VARIANT( "sl"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SL , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "usr"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 }) , asm_encode_c(OP_USR, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "usr"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6                  }) , asm_encode_c(OP_USR, o1.gpr, o2.gpr, GPR_ZR, o3.imm) ) \
    VARIANT( "usr"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_USR, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "isr"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 }) , asm_encode_c(OP_ISR, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "isr"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6                  }) , asm_encode_c(OP_ISR, o1.gpr, o2.gpr, GPR_ZR, o3.imm) ) \
    VARIANT( "isr"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_ISR, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    \
    VARIANT( "si.u" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 , ASM_OPERAND_U6 }) , asm_encode_b(OP_SI, o1.gpr, o2.gpr, o3.imm | (o4.imm << 6)) ) \
    VARIANT( "si.i" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 , ASM_OPERAND_U6 }) , asm_encode_b(OP_SI, o1.gpr, o2.gpr, o3.imm | (o4.imm << 6) | (1 << 12)) ) \
    \
    VARIANT( "mb"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 , ASM_OPERAND_U6 }) , asm_encode_b(OP_MB, o1.gpr, o2.gpr, o3.imm | (o4.imm << 6)) ) \
    VARIANT( "mb.i" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 , ASM_OPERAND_U6 }) , asm_encode_b(OP_MB, o1.gpr, o2.gpr, o3.imm | (o4.imm << 6) | (1 << 12)) ) \
    \
    VARIANT( "ror"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 }) , asm_encode_c(OP_ROR, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "ror"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6                  }) , asm_encode_c(OP_ROR, o1.gpr, o2.gpr, GPR_ZR, o3.imm) ) \
    VARIANT( "ror"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_ROR, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "rol"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 }) , asm_encode_c(OP_ROL, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "rol"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6                  }) , asm_encode_c(OP_ROL, o1.gpr, o2.gpr, GPR_ZR, o3.imm) ) \
    VARIANT( "rol"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_ROL, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    \
    VARIANT( "rev"     , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U6 }) , asm_encode_b(OP_REV, o1.gpr, o2.gpr, o3.imm  ) ) \
    VARIANT( "rev.h"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_b(OP_REV, o1.gpr, o2.gpr, 0b100000) ) \
    VARIANT( "rev.q"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_b(OP_REV, o1.gpr, o2.gpr, 0b110000) ) \
    VARIANT( "rev.b"   , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_b(OP_REV, o1.gpr, o2.gpr, 0b111000) ) \
    VARIANT( "rev.bit" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_b(OP_REV, o1.gpr, o2.gpr, 0b111111) ) \
    \
    VARIANT( "csb" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR }) , asm_encode_b(OP_CSB, o1.gpr, o2.gpr, 0) ) \
    VARIANT( "clz" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR }) , asm_encode_b(OP_CLZ, o1.gpr, o2.gpr, 0) ) \
    VARIANT( "ctz" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR }) , asm_encode_b(OP_CTZ, o1.gpr, o2.gpr, 0) ) \
    \
    VARIANT( "ext" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR }) , asm_encode_c(OP_EXT, o1.gpr, o2.gpr, o3.gpr, 0) ) \
    VARIANT( "dep" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR }) , asm_encode_c(OP_DEP, o1.gpr, o2.gpr, o3.gpr, 0) ) \
    \
    VARIANT( "seq"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SEQ , o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "seq"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I9 }) , asm_encode_c(OP_SEQ , o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "sult" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SULT, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "sult" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_SULT, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "silt" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SILT, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "silt" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I9 }) , asm_encode_c(OP_SILT, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "sule" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SULE, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "sule" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U9 }) , asm_encode_c(OP_SULE, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    VARIANT( "sile" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR                 }) , asm_encode_c(OP_SILE, o1.gpr, o2.gpr, o3.gpr, 0     ) ) \
    VARIANT( "sile" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I9 }) , asm_encode_c(OP_SILE, o1.gpr, o2.gpr, o3.gpr, o4.imm) ) \
    \
    VARIANT( "seqi"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I14 }) , asm_encode_b(OP_SEQI , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "sulti" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_SULTI, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "silti" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I14 }) , asm_encode_b(OP_SILTI, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "sulei" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_SULEI, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "silei" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_I14 }) , asm_encode_b(OP_SILEI, o1.gpr, o2.gpr, o3.imm) ) \
    \
    VARIANT( "bz" , G({ ASM_OPERAND_GPR, ASM_OPERAND_I19 }) , asm_encode_a(OP_BZ, o1.gpr, o2.imm) ) \
    VARIANT( "bn" , G({ ASM_OPERAND_GPR, ASM_OPERAND_I19 }) , asm_encode_a(OP_BN, o1.gpr, o2.imm) ) \
    \
    VARIANT( "jl"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_JL , o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "jl"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_U14                  }) , asm_encode_b(OP_JL , GPR_LP, o1.gpr, o2.imm) ) \
    VARIANT( "jl"  , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR                  }) , asm_encode_b(OP_JL , o1.gpr, o2.gpr, 0) ) \
    VARIANT( "jl"  , G({ ASM_OPERAND_GPR,                                  }) , asm_encode_b(OP_JL , GPR_LP, o1.gpr, 0) ) \
    VARIANT( "jlr" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR, ASM_OPERAND_U14 }) , asm_encode_b(OP_JLR, o1.gpr, o2.gpr, o3.imm) ) \
    VARIANT( "jlr" , G({ ASM_OPERAND_GPR, ASM_OPERAND_U14                  }) , asm_encode_b(OP_JLR, GPR_LP, o1.gpr, o2.imm) ) \
    VARIANT( "jlr" , G({ ASM_OPERAND_GPR, ASM_OPERAND_GPR                  }) , asm_encode_b(OP_JLR, o1.gpr, o2.gpr, 0) ) \
    VARIANT( "jlr" , G({ ASM_OPERAND_GPR,                                  }) , asm_encode_b(OP_JLR, GPR_LP, o1.gpr, 0) ) \
    \
    VARIANT( "syscall" , G({ }) , asm_encode_a(OP_SYSCALL, GPR_ZR, 0) ) \
    VARIANT( "breakpt" , G({ }) , asm_encode_a(OP_BREAKPT, GPR_ZR, 0) ) \
    VARIANT( "wait"    , G({ }) , asm_encode_a(OP_WAIT   , GPR_ZR, 0) ) \
    VARIANT( "spin"    , G({ }) , asm_encode_a(OP_SPIN   , GPR_ZR, 0) ) \
    VARIANT( "iret"    , G({ }) , asm_encode_a(OP_IRET   , GPR_ZR, 0) ) \
    \
    VARIANT( "lctrl" , G({ ASM_OPERAND_GPR, ASM_OPERAND_CTRL}) , asm_encode_a(OP_LCTRL, o1.gpr, o2.ctrl) ) \
    VARIANT( "sctrl" , G({ ASM_OPERAND_CTRL, ASM_OPERAND_GPR}) , asm_encode_a(OP_SCTRL, o2.gpr, o1.ctrl) ) \
    \
    VARIANT( "ret"    , G({                 }) , asm_encode_b(OP_JL, GPR_ZR, GPR_LP, 0) ) \
    VARIANT( "ret"    , G({ ASM_OPERAND_GPR }) , asm_encode_b(OP_JL, GPR_ZR, o1.gpr, 0) ) \

#endif // ASM_VARIANTS_H
