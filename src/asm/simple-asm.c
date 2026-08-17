#include "common/str.h"
#include "common/util.h"
#include "simple-asm.h"
#include "../system.h"
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// this is all terrible and should be replaced with some sort of interaction with the real assembler

typedef struct Parser {
    string line;
    u32 cursor;
} Parser;


static void advance(Parser* p) {
    p->cursor++;
}

static void advance_n(Parser* p, usize n) {
    p->cursor += n;
}

static bool is_eof(Parser* p) {
    return p->cursor >= p->line.len;
}

static char current(Parser* p) {
    if (is_eof(p)) {
        return 0;
    }
    return p->line.raw[p->cursor];
}

static void expect_advance(Parser* p, char c) {
    if (current(p) != c) {
        CRASH("expected character '%c', got '%c'", c, current(p));
    }
}

static char* current_ptr(Parser* p) {
    if (is_eof(p)) {
        return nullptr;
    }
    return &p->line.raw[p->cursor];
}

static void skip_whitespace(Parser* p) {
    while (!is_eof(p)) {
        switch (current(p)) {
        case ' ':
        case '\t':
        case '\v':
        case '\r':
            advance(p);
            break;
        default:
            return;
        }
    }
}

static string parse_ident(Parser* p) {
    skip_whitespace(p);
    if (!isalnum(current(p))) {
        CRASH("expected identifier");
    }
    string ident = {
        .raw = current_ptr(p),
        .len = 0,
    };
    while (isalnum(current(p)) || current(p) == '.') {
        advance(p);
        ident.len++;
    }

    // printf("PARSED IDENT ["str_fmt"]\n", str_arg(ident));
    return ident;
}

static AphelGpr get_gpr(string ident) {
    
    #define GPR(v, name) if (string_eq(strlit(name), ident)) return GPR_##v;
        APHEL_GPRS
    #undef GPR

    return GPR__COUNT;
}

static AphelGpr expect_gpr(Parser* p) {
    string ident = parse_ident(p);
    AphelGpr gpr = get_gpr(ident);    
    if (gpr == GPR__COUNT) {
        CRASH("expected gpr, got '"str_fmt"'", str_arg(ident));
    }
    return gpr;
}

static AphelCtrl get_ctrl(string ident) {
    
    #define CTRL(v, name) if (string_eq(strlit(name), ident)) return CTRL_##v;
        APHEL_CTRLS
    #undef CTRL

    return CTRL__COUNT;
}

static AphelCtrl expect_ctrl(Parser* p) {
    string ident = parse_ident(p);
    AphelCtrl ctrl = get_ctrl(ident);    
    if (ctrl == CTRL__COUNT) {
        CRASH("expected ctrl, got '"str_fmt"'", str_arg(ident));
    }
    return ctrl;
}

static i32 expect_int(Parser* p) {
    char* start = current_ptr(p);
    char* end;
    i64 value = (i64)strtoll(start, &end, 0);
    if (start == end) {
        CRASH("unable to parse numeric operand in '"str_fmt"'", str_arg(p->line));
    }
    if (value > INT32_MAX || value < INT32_MIN) {
        CRASH("operand with value %lld is too big", (long long) value);
    }
    advance_n(p, (intptr_t)end-(intptr_t)start);
    skip_whitespace(p);
    return (i32) value;
}

static AsmOperand parse_operand(Parser* p) {
    AsmOperand operand = {};

    skip_whitespace(p);

    if (current(p) == ',') {
        advance(p);
        skip_whitespace(p);
    }
    switch (current(p)) {
    case 'a' ... 'z': {
        string ident = parse_ident(p);

        AphelGpr gpr = get_gpr(ident);
        if (gpr != GPR__COUNT) {
            operand.kind = ASM_OPERAND_GPR;
            operand.gpr = gpr;
            break;
        }
        
        AphelCtrl ctrl = get_ctrl(ident);
        if (ctrl != CTRL__COUNT) {
            operand.kind = ASM_OPERAND_CTRL;
            operand.ctrl = ctrl;
            break;
        }

        CRASH("unable to categorize operand '"str_fmt"'", str_arg(ident));
    }
    case '0' ... '9':
    case '-': {
        operand.kind = ASM_OPERAND_IMM;
        operand.imm = expect_int(p);
        break;
    }
    case '[': {
        operand.kind = ASM_OPERAND_MEM;
        operand.mem.r1 = expect_gpr(p);
        skip_whitespace(p);
        if (current(p) == '+') {
            advance(p);
            skip_whitespace(p);
            if (isalpha(current(p))) {
                operand.mem.r2 = expect_gpr(p);
                if (current(p) == '+') {
                    advance(p);
                    skip_whitespace(p);
                    operand.mem.i = expect_int(p);
                }
            } else {
                operand.mem.i = expect_int(p);
            }
        }
        expect_advance(p, ']');
        break;
    }
    default:
        CRASH("unable to parse operand in '"str_fmt"'", str_arg(p->line));
    }

    return operand;
}

static AsmParsedInst parse_line(string line) {
    Parser* p = &(Parser){
        .line = line,
        .cursor = 0,
    };


    AsmParsedInst inst = {
        .name = parse_ident(p),
    };

    skip_whitespace(p);
    
    if (!is_eof(p)) {
        inst.operands[0] = parse_operand(p);
    }
    
    if (!is_eof(p)) {
        inst.operands[1] = parse_operand(p);
    }
    
    if (!is_eof(p)) {
        inst.operands[2] = parse_operand(p);
    }
    
    if (!is_eof(p)) {
        inst.operands[3] = parse_operand(p);
    }

    if (!is_eof(p)) {
        CRASH("too many operands in '"str_fmt"'", str_arg(line));
    }

    return inst;
}


u32 asm_encode_a(AphelOpcode op, AphelGpr r1, i32 imm19) {
    EncodedInst inst = {.A = {
        .opcode = op,
        .r1 = r1,
        .imm = imm19,
    }};
    return inst.bits;
}

u32 asm_encode_b(AphelOpcode op, AphelGpr r1, AphelGpr r2, i32 imm14)  {
    EncodedInst inst = {.B = {
        .opcode = op,
        .r1 = r1,
        .r2 = r2,
        .imm = imm14,
    }};
    return inst.bits;
}

u32 asm_encode_c(AphelOpcode op, AphelGpr r1, AphelGpr r2, AphelGpr r3, i32 imm9)  {
    EncodedInst inst = {.C = {
        .opcode = op,
        .r1 = r1,
        .r2 = r2,
        .r3 = r3,
        .imm = imm9,
    }};
    return inst.bits;
}

static bool match_operand_template(AsmOperandKind template, AsmOperand op) {
    switch (template) {
    case ASM_OPERAND_GPR:
    case ASM_OPERAND_CTRL:
        return op.kind == template;

    case ASM_OPERAND_MEM_64:
        return op.kind == ASM_OPERAND_MEM 
            && 0 <= op.mem.i && op.mem.i <= 4088 
            && op.mem.i % 8 == 0;
    case ASM_OPERAND_MEM_32:
        return op.kind == ASM_OPERAND_MEM 
            && 0 <= op.mem.i && op.mem.i <= 2044 
            && op.mem.i % 4 == 0;
    case ASM_OPERAND_MEM_16:
        return op.kind == ASM_OPERAND_MEM 
            && 0 <= op.mem.i && op.mem.i <= 1022 
            && op.mem.i % 2 == 0;
    case ASM_OPERAND_MEM_8:
        return op.kind == ASM_OPERAND_MEM 
            && 0 <= op.mem.i && op.mem.i <= 511;

    case ASM_OPERAND_MEM_SM_64:
        return op.kind == ASM_OPERAND_MEM 
            && op.mem.r2 == GPR_ZR 
            && 0 <= op.mem.i && op.mem.i <= 4088 
            && op.mem.i % 8 == 0;
    case ASM_OPERAND_MEM_SM_32:
        return op.kind == ASM_OPERAND_MEM 
            && op.mem.r2 == GPR_ZR 
            && 0 <= op.mem.i && op.mem.i <= 2044 
            && op.mem.i % 4 == 0;
    case ASM_OPERAND_MEM_SM_16:
        return op.kind == ASM_OPERAND_MEM 
            && op.mem.r2 == GPR_ZR 
            && 0 <= op.mem.i && op.mem.i <= 1022 
            && op.mem.i % 2 == 0;
    case ASM_OPERAND_MEM_SM_8:
        return op.kind == ASM_OPERAND_MEM 
            && op.mem.r2 == GPR_ZR 
            && 0 <= op.mem.i && op.mem.i <= 511;
    
    case ASM_OPERAND_I9:
        return op.kind == ASM_OPERAND_IMM
            && -256 <= op.imm && op.imm <= 255;
    case ASM_OPERAND_I14:
        return op.kind == ASM_OPERAND_IMM
            && -8192 <= op.imm && op.imm <= 8191;
    case ASM_OPERAND_I19:
        return op.kind == ASM_OPERAND_IMM
            && -262144 <= op.imm && op.imm <= 262143;
    case ASM_OPERAND_U9:
        return op.kind == ASM_OPERAND_IMM
            && 0 <= op.imm && op.imm <= 511;
    case ASM_OPERAND_U14:
        return op.kind == ASM_OPERAND_IMM
            && 0 <= op.imm && op.imm <= 16383;
    case ASM_OPERAND_U19:
        return op.kind == ASM_OPERAND_IMM
            && 0 <= op.imm && op.imm <= 524287;

    case ASM_OPERAND_U6:
        return op.kind == ASM_OPERAND_IMM
            && 0 <= op.imm && op.imm <= 63;
    case ASM_OPERAND_U16:
        return op.kind == ASM_OPERAND_IMM
            && 0 <= op.imm && op.imm <= 65535;
    case ASM_OPERAND_SSI_SHIFT:
        return op.kind == ASM_OPERAND_IMM
            && 0 <= op.imm && op.imm < 64
            && op.imm % 16 == 0;
    default:
        CRASH("unhandled operand template kind");
    }
}

u32 encode_inst(string line) {
    AsmParsedInst inst = parse_line(line);

    u32 num_operands = 4;
    for_n(i, 0, 4) {
        if (inst.operands[i].kind == ASM_OPERAND_NONE) {
            num_operands = i;
            break;
        }
    }

    AsmOperand o1 = inst.operands[0];
    AsmOperand o2 = inst.operands[1];
    AsmOperand o3 = inst.operands[2];
    AsmOperand o4 = inst.operands[3];

    // actually execute me
    #define G(...) __VA_ARGS__
    #define VARIANT(variant_name, args, expr)                                               \
        if (string_eq(strlit(variant_name), inst.name)) {                                   \
            AsmOperandKind operand_templates[] = args;                                      \
            if (sizeof(operand_templates) / sizeof(AsmOperandKind) == num_operands) {       \
                bool operands_match = true;                                                 \
                for (int i = 0; i < num_operands; i++) {                                    \
                    if (!match_operand_template(operand_templates[i], inst.operands[i])) {  \
                        operands_match = false;                                             \
                        break;                                                              \
                    }                                                                       \
                }                                                                           \
                if (operands_match) {                                                       \
                    return expr;                                                            \
                }                                                                           \
            }                                                                               \
        }

        INST_VARIANTS
    #undef VARIANT
    #undef G

    printf("cannot find a match for: "str_fmt" ", str_arg(inst.name));
    for_n(i, 0, num_operands) {
        if (i != 0) {
            printf(", ");
        }
        switch (inst.operands[i].kind) {
        case ASM_OPERAND_GPR:
            printf("<gpr>");
            break;
        case ASM_OPERAND_CTRL:
            printf("<ctrl>");
            break;
        case ASM_OPERAND_MEM:
            printf("<mem>");
            break;
        case ASM_OPERAND_IMM:
            printf("<imm>");
            break;
        default:
            printf("?");
        }
    }
    exit(1);

    // CRASH("could not find a match for instruction '"str_fmt"'", str_arg(inst));
}
