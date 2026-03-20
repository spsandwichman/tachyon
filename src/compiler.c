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
        u8 hreg_index : 4;

        bool inside_hreg : 1;
        bool non_zero    : 1;
        bool write_safe  : 1;
        bool read_safe   : 1;
        bool modified    : 1;
    } sreg_status[GPR__COUNT];

    /// The most recent definition of an hreg.
    UOpIndex hreg_def[HREG_ALLOC_SET_LEN];

    /// The most recent definition of an hreg.
    AphelGpr hreg_repr[HREG_ALLOC_SET_LEN];

} Compiler;

static void assign_sreg_to_hreg(Compiler* c, AphelGpr sreg, u8 hreg, UOpIndex definition) {
    c->sreg_status[sreg].inside_hreg = true;
    c->sreg_status[sreg].hreg_index = hreg;
    c->hreg_repr[hreg] = sreg;
    c->hreg_def[hreg] = definition;
}

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

static UOpIndex uop_make(Compiler* c, UOpKind kind, u8 hreg_out, AphelGpr sreg_out, u16 extra, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .hreg = hreg_out,
        .extra = extra,
        .src1 = UOP_INDEX_NULL,
        .src2 = UOP_INDEX_NULL,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    if (hreg_out != HREG_NONE) {
        if (kind != UOP_GET_SREG) {
            c->sreg_status[sreg_out].modified = true;
        }
        assign_sreg_to_hreg(c, sreg_out, hreg_out, index);
    }
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

static UOpIndex uop_make1(Compiler* c, UOpKind kind, UOpIndex src1, u8 hreg_out, AphelGpr sreg_out, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .hreg = hreg_out,
        .extra = 0,
        .src1 = src1,
        .src2 = UOP_INDEX_NULL,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    if (hreg_out != HREG_NONE) {
        c->sreg_status[sreg_out].modified = true;
        assign_sreg_to_hreg(c, sreg_out, hreg_out, index);
    }
    return index;
}

static UOpIndex uop_make1_ex(Compiler* c, UOpKind kind, UOpIndex src1, u8 hreg_out, AphelGpr sreg_out, u16 extra, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .hreg = hreg_out,
        .extra = extra,
        .src1 = src1,
        .src2 = UOP_INDEX_NULL,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    if (hreg_out != HREG_NONE) {
        if (kind != UOP_GET_SREG) {
            c->sreg_status[sreg_out].modified = true;
        }
        assign_sreg_to_hreg(c, sreg_out, hreg_out, index);
    }
    return index;
}

static UOpIndex uop_make2(Compiler* c, UOpKind kind, UOpIndex src1, UOpIndex src2, u8 hreg_out, AphelGpr sreg_out, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .hreg = hreg_out,
        .extra = 0,
        .src1 = src1,
        .src2 = src2,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    if (hreg_out != HREG_NONE) {
        c->sreg_status[sreg_out].modified = true;
        assign_sreg_to_hreg(c, sreg_out, hreg_out, index);
    }
    return index;
}

static UOpIndex uop_make2_ex(Compiler* c, UOpKind kind, UOpIndex src1, UOpIndex src2, u8 hreg_out, AphelGpr sreg_out, u16 extra, u32 imm32) {
    UOp uop = {
        .kind = kind,
        .hreg = hreg_out,
        .extra = extra,
        .src1 = src1,
        .src2 = src2,
        .imm32 = imm32,
    };

    UOpIndex index = vec_len(c->uops);
    vec_append(&c->uops, uop);
    if (hreg_out != HREG_NONE) {
        c->sreg_status[sreg_out].modified = true;
        assign_sreg_to_hreg(c, sreg_out, hreg_out, index);
    }
    return index;
}

static u8 create_free_hreg(Compiler* c) {

    // evict the least recently defined sreg from its hreg.
    // this is information we already have access to, and it's
    // probably a good enough heuristic for LRU (?)
    // TODO make this actually least-recently-used
    u8 selected_hreg = 0;
    for_n(i, 1, HREG_ALLOC_SET_LEN) {
        if (c->hreg_def[i] <= c->hreg_def[selected_hreg]) {
            selected_hreg = i;
        }
    }

    // evict some sreg we dont want anymore back to register bank
    AphelGpr evicted_sreg = c->hreg_repr[selected_hreg];
    
    if (evicted_sreg != GPR__COUNT) {
        if (c->sreg_status[evicted_sreg].modified) {
            uop_make1_ex(c, 
                UOP_PUT_SREG, 
                c->hreg_def[selected_hreg], 
                HREG_NONE, 0,
                evicted_sreg,
                0
            );
        }
        c->sreg_status[evicted_sreg].inside_hreg = false;
    }

    return selected_hreg;
}

static void evict_all_active(Compiler* c) {
    for_n(hreg, 1, HREG_ALLOC_SET_LEN) {
        if (c->hreg_def[hreg] == UOP_INDEX_NULL) {
            continue;
        }

        AphelGpr evicted_sreg = c->hreg_repr[hreg];
        if (c->sreg_status[evicted_sreg].modified) {
            uop_make1_ex(c, 
                UOP_PUT_SREG, 
                c->hreg_def[hreg], 
                HREG_NONE, 0,
                evicted_sreg,
                0
            );
        }
        c->sreg_status[evicted_sreg].inside_hreg = false;
        c->sreg_status[evicted_sreg].hreg_index = HREG_NONE;

    }
}

static u8 get_destination_hreg(Compiler* c, AphelGpr sreg) {
    // if it's already inside a hardware register, retrieve that
    if (c->sreg_status[sreg].inside_hreg) {
        return c->sreg_status[sreg].hreg_index;
    }

    // otherwise, we have to fill up another hardware register.
    u8 selected_hreg = create_free_hreg(c);
    return selected_hreg;
}

static UOpIndex retrieve_sreg(Compiler* c, AphelGpr sreg) {
    DEBUG("  retrieving '%s' ... ", gpr_name[sreg]);

    // if it's already inside a hardware register, retrieve that
    if (c->sreg_status[sreg].inside_hreg) {
        u8 hreg = c->sreg_status[sreg].hreg_index;
        DEBUG("found in h%d\n", hreg);
        return c->hreg_def[hreg];
    }

    // otherwise, we have to fill up another hardware register.
    u8 selected_hreg = create_free_hreg(c);

    // load the sreg we want into the hreg we just freed up
    DEBUG("loading to h%d\n", selected_hreg);

    UOpIndex retrieve = uop_make(c,
        UOP_GET_SREG,
        selected_hreg,
        sreg, sreg,
        0
    );

    return retrieve;
}

typedef struct SrcOperands {
    UOpIndex src1; 
    UOpIndex src2;
} SrcOperands;

static SrcOperands retrieve_operands(Compiler* c, EncodedInst inst) {
    UOpIndex src1 = UOP_INDEX_NULL;
    UOpIndex src2 = UOP_INDEX_NULL;

    // retrieve source operands, if necessary
    switch (inst.opcode) {
    case OP_WAIT:
        break;
    case OP_SSI: {
        bool clear = inst.A.imm & 1;
        // usize shift = (inst.A.imm >> 1) & 0b11;
        if (!clear) {
            src1 = retrieve_sreg(c, inst.A.r1);
        }
        break;
    }
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_UDIV:
    case OP_IDIV: {
        src1 = retrieve_sreg(c, inst.C.r2);
        src2 = retrieve_sreg(c, inst.C.r3);
        break;
    }
    case OP_BZ:
    case OP_BN: {
        if (inst.A.r1 != GPR_ZR) {
            src1 = retrieve_sreg(c, inst.A.r1);
        }
        break;
    }
    default:
        TODO("failed to retrieve operands");
    }

    return (SrcOperands){
        .src1 = src1,
        .src2 = src2,
    };
}

/// Compile the next instruction in the sequence, updating the
/// Compiler struct with state relating to the current compilation.
/// \return true if decoding should continue after this call, false if not.
static bool compile_next_inst(Compiler* c) {
    EncodedInst encoded_inst = c->binary[c->inst_index];
    AphelOpcode opcode = encoded_inst.opcode;

    DEBUG("compiling inst '%s' (%d)\n", op_name[opcode], opcode);

    // get operands
    SrcOperands operands = retrieve_operands(c, encoded_inst);

    // select UOps
    switch (opcode) {
    case OP_SSI: {
        bool clear = encoded_inst.A.imm & 1;
        usize shift = 16 * ((encoded_inst.A.imm >> 1) & 0b11);
        i32 value = encoded_inst.A.imm >> 3;
        value <<= 16;
        value >>= 16;
        if (clear) {
            if (shift <= 32) {
                value = value << shift;
            } else {
                TODO("big SSI");
            }

            // get an output register
            u8 output_hreg = get_destination_hreg(c, encoded_inst.A.r1);
            UOpIndex load_const = uop_make(c, 
                UOP_SET_I32,
                output_hreg,
                encoded_inst.A.r1,
                0, 
                value
            );
        } else {
            TODO("non-clear SSI");
        }
        break;
    }
    case OP_ADD: {
        u8 output_hreg = get_destination_hreg(c, encoded_inst.C.r1);
        UOpIndex output = uop_make2(c, UOP_ADD, operands.src1, operands.src2, output_hreg, encoded_inst.C.r1, 0);
        break;
    }
    case OP_SUB: {
        u8 output_hreg = get_destination_hreg(c, encoded_inst.C.r1);
        UOpIndex output = uop_make2(c, UOP_SUB, operands.src1, operands.src2, output_hreg, encoded_inst.C.r1, 0);
        break;
    }
    case OP_MUL: {
        u8 output_hreg = get_destination_hreg(c, encoded_inst.C.r1);
        UOpIndex output = uop_make2(c, UOP_MUL, operands.src1, operands.src2, output_hreg, encoded_inst.C.r1, 0);
        break;
    }
    case OP_WAIT: {
        evict_all_active(c);
        BlockExitCode code = BLOCK_EXIT_NEXT;
        if (c->lp->sys->flags.mode_test) {
            code = BLOCK_EXIT_TEST_QUIT;
        }
        uop_make(c, UOP_EXIT, HREG_NONE, 0, code, 0);
        break;
    }
    case OP_BZ: {
        // if (encoded_inst.A.r1 != GPR_ZR) {
        //     // this branch is actually conditional, FUCK
        //     TODO("make this work lmao");
        // }
        // UOpIndex ip = retrieve_sreg(c, GPR_IP);
        // u8 output_hreg = get_destination_hreg(c, GPR_IP);
        i32 imm = encoded_inst.A.imm << (32 - 19) >> (32 - 19);
        // UOpIndex addition = uop_make1(c, 
        //     UOP_ADD_I32, 
        //     ip, 
        //     output_hreg, 
        //     GPR_IP,
        //     imm
        // );
        evict_all_active(c);

        if (encoded_inst.A.r1 == GPR_ZR) {
            uop_make1(c, UOP_B, operands.src1, HREG_NONE, 0, imm);
        } else {
            uop_make(c, UOP_BZ, HREG_NONE, 0, 0, imm);
        }


        break;
    }
    default:
        TODO("failed to emit uops");
    }

    c->inst_index += 1;

    return !stop_at_inst(encoded_inst);
}

static void dump_uops(Vec(UOp) uops);

UOpBlock* compile_block(Lp* lp, EncodedInst* binary) {
    Compiler c = {
        .lp = lp,
        .binary = binary,
        .inst_index = 0,
        .uops = vec_new(UOp, 128),
        .sreg_status = {},
        .hreg_def = {},
    };
    for_n(i, 0, HREG_ALLOC_SET_LEN) {
        c.hreg_repr[i] = GPR__COUNT;
    }

    // insert a NOP so that index 0 can be used as a 'null' index value.
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

    case UOP_ADD_I32: return "add-i32";

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

static void print_hreg(u8 hreg) {
    printf(Bold Red"h%u "Reset, hreg);
}

static void print_uop(Vec(UOp) uops, UOpIndex index, UOp uop) {
    printf(" % 3d: ", index);
    if (uop.hreg != HREG_NONE) {
        print_hreg(uop.hreg);
        printf("= ");
    } else {
        printf("     ");

    }
    printf("%s ", uop_name(uop));

    switch (uop.kind) {
    case UOP_GET_SREG:
        print_gpr(uop.extra);
        break;
    case UOP_PUT_SREG:
        print_gpr(uop.extra);
        print_hreg(uops[uop.src1].hreg);
        break;
    case UOP_SET_I32:
        printf("0x%x", uop.imm32);
        break;
    case UOP_ADD:
    case UOP_SUB:
    case UOP_MUL:
        print_hreg(uops[uop.src1].hreg);
        print_hreg(uops[uop.src2].hreg);
        break;
    case UOP_ADD_I32:
        print_hreg(uops[uop.src1].hreg);
        printf("0x%x", uop.imm32);
        break;
    case UOP_BZ:
    case UOP_BN:
        print_hreg(uops[uop.src1].hreg);
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
