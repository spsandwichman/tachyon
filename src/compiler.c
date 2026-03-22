#include "common/util.h"
#include "common/ansi.h"
#include "aphelion.h"
#include "compiler.h"
#include "system.h"
#include <stdio.h>

typedef struct Compiler {
    Lp* lp;
    Vec(UOp) uops;

    EncodedInst* binary;
    u32 inst_index;
    struct {
        // most recent definition of this sreg,
        // or UOP_INDEX_NULL if not defined in this block.
        UOpIndex recent_def;

        bool non_zero    : 1;
        bool write_safe  : 1;
        bool read_safe   : 1;
        bool modified    : 1;
    } sreg_status[GPR__COUNT];


} Compiler;

static bool stop_at_inst(EncodedInst encoded_inst) {
    switch (encoded_inst.opcode) {
    case OP_CINVAL:
    case OP_JLR:
    case OP_JL:
    case OP_BZ:
    case OP_BN:
    case OP_SYSCALL:
    case OP_BREAKPT:
    case OP_WAIT:
        return true;
    default:
        return false;
    }
}

static UOpIndex uop_make(Compiler* c, UOpKind kind, u16 extra, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .extra = extra,
        .src1 = UOP_INDEX_NULL,
        .src2 = UOP_INDEX_NULL,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    return index;
}

// static UOpIndex uop_make_imm(Compiler* c, UOpKind kind, u8 hreg_out, AphelGpr sreg_out, u16 extra, u32 imm) {
//     UOp uop = {
//         .kind = kind,
//         .hreg = hreg_out,
//         .extra = extra,
//         .imm32 = imm,
//     };

//     UOpIndex index = vec_len(c->uops);
//     vec_append(&c->uops, uop);
//     if (hreg_out != HREG_NONE) {
//         c->sreg_status[sreg_out].modified = true;
//         assign_sreg_to_hreg(c, sreg_out, hreg_out, index);
//     }
//     return index;
// }

static UOpIndex uop_make1(Compiler* c, UOpKind kind, UOpIndex src1, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .extra = 0,
        .src1 = src1,
        .src2 = UOP_INDEX_NULL,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    return index;
}

static UOpIndex uop_make1_ex(Compiler* c, UOpKind kind, UOpIndex src1, u16 extra, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .extra = extra,
        .src1 = src1,
        .src2 = UOP_INDEX_NULL,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    return index;
}

static UOpIndex uop_make2(Compiler* c, UOpKind kind, UOpIndex src1, UOpIndex src2, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .extra = 0,
        .src1 = src1,
        .src2 = src2,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    return index;
}

static UOpIndex uop_make2_ex(Compiler* c, UOpKind kind, UOpIndex src1, UOpIndex src2, u16 extra, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .extra = extra,
        .src1 = src1,
        .src2 = src2,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    return index;
}

static void dump_uops(Vec(UOp) uops);

static UOpIndex get_sreg(Compiler* c, AphelGpr sreg) {
    if (c->sreg_status[sreg].recent_def != UOP_INDEX_NULL) {
        return c->sreg_status[sreg].recent_def;
    }
    UOpIndex get_sreg = uop_make(c, UOP_GET_SREG, sreg, 0);
    c->sreg_status[sreg].recent_def = get_sreg;
    return get_sreg;
}

static void set_sreg(Compiler* c, AphelGpr sreg, UOpIndex uop) {
    c->sreg_status[sreg].modified = true;
    c->sreg_status[sreg].recent_def = uop;
}

static void put_all(Compiler* c, bool invalidate) {
    for_n(sreg, 0, GPR__COUNT) {
        auto* status = &c->sreg_status[sreg];
        if (status->modified) {
            uop_make(c, UOP_PUT_SREG, sreg, 0);

            if (invalidate) {
                status->recent_def = UOP_INDEX_NULL;
            }
        }
    }
}

static bool compile_next_inst(Compiler* c) {

    

    return false;
}

UOpBlock* compile_block(System* sys, Lp* lp, EncodedInst* binary) {
    Compiler c = {
        .lp = lp,
        .binary = binary,
        .inst_index = 0,
        .uops = vec_new(UOp, 128),
        .sreg_status = {},
    };

    // insert a NOP so that index 0 can be used as a null value.
    vec_append(&c.uops, (UOp){.kind = UOP_NOP});

    while (compile_next_inst(&c)) {}

    dump_uops(c.uops);

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

static const char* uop_name(UOp uop) {
    switch (uop.kind) {
    case UOP_NOP: return "nop";

    case UOP_GET_SREG: return "get-sreg";
    case UOP_PUT_SREG: return "put-sreg";

    case UOP_SET_I32: return "set-i32";
    case UOP_SET_U32: return "set-u32";

    case UOP_VADDR_TRANSLATE: return "vaddr-translate";
    case UOP_BUS_VERIFY: return "bus-verify";
    
    case UOP_BUS_RAW_READ_8:   return "bus-read-8";
    case UOP_BUS_RAW_READ_16:  return "bus-read-16";
    case UOP_BUS_RAW_READ_32:  return "bus-read-32";
    case UOP_BUS_RAW_READ_64:  return "bus-read-64";
    case UOP_BUS_RAW_WRITE_8:  return "bus-write-8";
    case UOP_BUS_RAW_WRITE_16: return "bus-write-16";
    case UOP_BUS_RAW_WRITE_32: return "bus-write-32";
    case UOP_BUS_RAW_WRITE_64: return "bus-write-64";

    case UOP_ADD: return "add";
    case UOP_SUB: return "sub";
    case UOP_MUL: return "mul";
    case UOP_UDIV: return "udiv";
    case UOP_UDIV_UNCHECKED: return "udiv-unchecked";
    case UOP_IDIV: return "idiv";
    case UOP_IDIV_UNCHECKED: return "idiv-unchecked";

    case UOP_BZ: return "bz";
    case UOP_BN: return "bn";
    case UOP_B: return "b";

    case UOP_EXIT: return "exit";
    default:
        TODO("unhandled uop %d", uop.kind);
    }
}

static void print_gpr(AphelGpr gpr) {
    printf(Bold Cyan"%s "Reset, gpr_name[gpr]);
}

static void print_opindex(u8 hreg) {
    printf(Bold Red"h%u "Reset, hreg);
}

static void print_uop(Vec(UOp) uops, UOpIndex index, UOp uop) {
    printf(" % 3d = %s ", index, uop_name(uop));

    switch (uop.kind) {
    case UOP_GET_SREG:
        print_gpr(uop.extra);
        break;
    case UOP_PUT_SREG:
        print_gpr(uop.extra);
        break;
    case UOP_SET_I32:
        printf("0x%x", uop.imm32);
        break;
    case UOP_ADD:
    case UOP_SUB:
    case UOP_MUL:
        print_opindex(uop.src1);
        print_opindex(uop.src2);
        break;
    case UOP_BZ:
    case UOP_BN:
        print_opindex(uop.src1);
        printf("%d", uop.imm32);
        break;
    case UOP_B:
        printf("%d", uop.imm32);
        break;
    case UOP_EXIT:
        break;
    default:
        TODO("unhandled uop %d", uop.kind);
    }

    printf("\n");
}

static void dump_uops(Vec(UOp) uops) {
    printf("dump block %p\n", uops);
    for_n(i, 1, vec_len(uops)) {
        print_uop(uops, i, uops[i]);
    }
}
