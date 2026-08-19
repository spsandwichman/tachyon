#include "common/type.h"
#include "common/util.h"
#include "common/ansi.h"
#include "aphelion.h"
#include "system.h"
#include "targets/x64.h"

#include <sys/mman.h>
#include <unistd.h>

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

static void set_sreg(Compiler* c, AphelGpr sreg, UOpIndex uop) {
    if (sreg == GPR_ZR || sreg == GPR_IP) {
        return;
    }
    c->sreg_status[sreg].modified = true;
    c->sreg_status[sreg].recent_def = uop;
}

static void set_sreg_unsafe(Compiler* c, AphelGpr sreg, UOpIndex uop) {
    c->sreg_status[sreg].modified = true;
    c->sreg_status[sreg].recent_def = uop;
}

static UOpIndex get_sreg(Compiler* c, AphelGpr sreg) {
    if (sreg == GPR_IP) {
        UOpIndex old_ip;
        if (c->sreg_status[GPR_IP].recent_def != UOP_INDEX_NULL) {
            old_ip = c->sreg_status[GPR_IP].recent_def;
        } else {
            old_ip = uop_make(c, UOP_GET_SREG, GPR_IP, 0);
        }

        usize offset_from_block_start = (c->cursor * 4 + 4);
        // offset which has to be added to the last-updated value of IP
        // to be accurate
        UOpIndex new_ip = uop_make1(c, UOP_ADD_I32, old_ip, offset_from_block_start - c->ip.offset_start);
        c->ip.offset_start = offset_from_block_start;
        // c->sreg_status[sreg].recent_def = new_ip;
        set_sreg_unsafe(c, GPR_IP, new_ip);
        return new_ip;
    }

    if (c->sreg_status[sreg].recent_def != UOP_INDEX_NULL) {
        return c->sreg_status[sreg].recent_def;
    }
    UOpIndex get_sreg = uop_make(c, UOP_GET_SREG, sreg, 0);
    c->sreg_status[sreg].recent_def = get_sreg;
    return get_sreg;
}

static void flush_all(Compiler* c, bool invalidate) {
    // update IP

    for_n(sreg, 0, GPR__COUNT) {
        if (sreg == GPR_IP) {
            get_sreg(c, GPR_IP);
        }
        auto status = &c->sreg_status[sreg];
        if (status->modified) {
            uop_make1_ex(c, UOP_PUT_SREG, status->recent_def, sreg, 0);

            if (invalidate) {
                status->recent_def = UOP_INDEX_NULL;
            }
        }
    }
}


static void debug_inst_binary(EncodedInst einst) {
    AphelFmt fmt = fmt_from_op(einst.opcode);
    
    switch (fmt) {
    case FMT_A:
        DEBUG_NOI(Cyan"%09b ", (einst.A.imm >> 10) & ((1 << 9) - 1));
        DEBUG_NOI("%05b ", (einst.A.imm >> 5) & ((1 << 5) - 1));
        DEBUG_NOI("%05b ", (einst.A.imm) & ((1 << 5) - 1));
        DEBUG_NOI(Red"%05b ", einst.A.r1);
        break;
    case FMT_B:
        DEBUG_NOI(Cyan"%09b ", (einst.B.imm >> 5) & ((1 << 9) - 1));
        DEBUG_NOI("%05b ", (einst.B.imm) & ((1 << 5) - 1));
        DEBUG_NOI(Red"%05b ", einst.B.r2);
        DEBUG_NOI("%05b ", einst.B.r1);
        break;
    case FMT_C:
        DEBUG_NOI(Cyan "%09b ", einst.C.imm);
        DEBUG_NOI(Red "%05b ", einst.C.r3);
        DEBUG_NOI("%05b ", einst.C.r2);
        DEBUG_NOI("%05b ", einst.C.r1);
        break;
    }

    DEBUG_NOI(Yellow"%06b""%02b"Reset, einst.opcode >> 2, einst.opcode & 0b11);
}

static bool compile_next_inst(Compiler* c) {
    EncodedInst einst = c->binary[c->cursor];

    DEBUG("compiling %-7s ", op_name[einst.opcode]);
    debug_inst_binary(einst);
    DEBUG("\n");

    switch (einst.opcode) {
    case OP_ADD: {
        UOpIndex r2 = get_sreg(c, einst.C.r2);
        UOpIndex r3 = get_sreg(c, einst.C.r3);
        UOpIndex op = uop_make2(c, UOP_ADD, r2, r3, 0);
        if (einst.C.imm != 0) {
            op = uop_make1(c, UOP_ADD_I32, op, einst.C.imm);
        }
        set_sreg(c, einst.C.r1, op);
        break;
    }
    case OP_SUB: {
        UOpIndex r2 = get_sreg(c, einst.C.r2);
        UOpIndex r3 = get_sreg(c, einst.C.r3);
        UOpIndex op = uop_make2(c, UOP_SUB, r2, r3, 0);
        if (einst.C.imm != 0) {
            op = uop_make1(c, UOP_SUB_I32, op, einst.C.imm);
        }
        set_sreg(c, einst.C.r1, op);
        break;
    }
    case OP_MUL: {
        UOpIndex r2 = get_sreg(c, einst.C.r2);
        UOpIndex r3 = get_sreg(c, einst.C.r3);
        if (einst.C.imm != 0) {
            r3 = uop_make1(c, UOP_ADD_I32, r3, einst.C.imm);
        }
        UOpIndex op = uop_make2(c, UOP_MUL, r2, r3, 0);
        set_sreg(c, einst.C.r1, op);
        break;
    }
    case OP_SSI: {
        bool clear = einst.A.imm & 1;
        u8 shift = ((einst.A.imm >> 1) & 0b11) * 16;
        i32 val = ((einst.A.imm >> 3) << 16) >> 16;
        if (clear) {
            if (shift <= 32) {
                UOpIndex op = uop_make(c, UOP_SET_I32, 0, val);
                set_sreg(c, einst.A.r1, op);
            } else {
                TODO();
            }
        } else {
            TODO();
        }
        break;
    }
    case OP_WAIT: {
        flush_all(c, true);
        uop_make(c, UOP_EXIT, EXIT_QUIT, 0);
        return false;
    }
    case OP_BZ: {
        if (einst.A.r1 == GPR_ZR) {
            UOpIndex ip = get_sreg(c, GPR_IP);

            UOpIndex constant = uop_make(c, UOP_SET_I32, 0, 
                ((i32)einst.A.imm << 13) >> 11
            );
            UOpIndex add = uop_make2(c, UOP_ADD, ip, constant, 0);

            set_sreg_unsafe(c, GPR_IP, add);
            
            flush_all(c, true);
            uop_make(c, UOP_EXIT, EXIT_NEXT_BLOCK, 0);
        } else {
            TODO();
        }
        return false;
    }
    default:
        TODO("unknown instruction");
    }

    c->cursor += 1;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

static const char* uop_name(UOp uop) {
    switch (uop.kind) {
    case UOP_NOP: return "nop";

    case UOP_PARAM_LP: return "param-lp";
    case UOP_PARAM_TABLE: return "param-table";

    case UOP_GET_SREG: return "get-sreg";
    case UOP_PUT_SREG: return "put-sreg";

    case UOP_SET_I32: return "set-i32";

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
    case UOP_SUB_I32: return "sub-i32";

    case UOP_BZ: return "bz";
    case UOP_BN: return "bn";
    case UOP_B: return "b";

    case UOP_EXIT: return "exit";
    default:
        TODO("unhandled uop %d", uop.kind);
    }
}

static void debug_gpr(AphelGpr gpr) {
    DEBUG_NOI(Bold Cyan"%s "Reset, gpr_name[gpr]);
}

static void debug_opindex(UOpIndex hreg) {
    DEBUG_NOI( Red"t%u "Reset, hreg);
}

static void debug_uop(Vec(UOp) uops, UOpIndex index, UOp uop) {
    DEBUG("");
    DEBUG_NOI( Red"t%-3u "Reset, index);
    DEBUG_NOI("| %s ", uop_name(uop));

    switch (uop.kind) {
    case UOP_PARAM_LP:
    case UOP_PARAM_TABLE:
        break;
    case UOP_GET_SREG:
        debug_gpr(uop.extra);
        break;
    case UOP_PUT_SREG:
        debug_gpr(uop.extra);
        debug_opindex(uop.src1);
        break;
    case UOP_SET_I32:
        DEBUG_NOI("%d", uop.imm32);
        break;
    case UOP_ADD:
    case UOP_SUB:
    case UOP_MUL:
        debug_opindex(uop.src1);
        debug_opindex(uop.src2);
        break;
    case UOP_ADD_I32:
    case UOP_SUB_I32:
        debug_opindex(uop.src1);
        DEBUG_NOI("%d", uop.imm32);
        break;
    case UOP_BZ:
    case UOP_BN:
        debug_opindex(uop.src1);
        DEBUG_NOI("%d", uop.imm32);
        break;
    case UOP_B:
        DEBUG_NOI("%d", uop.imm32);
        break;
    case UOP_EXIT:
        switch (uop.extra) {
        case EXIT_NEXT_BLOCK:
            DEBUG_NOI("next");
            break;
        case EXIT_QUIT:
            DEBUG_NOI("quit");
            break;
        case EXIT_INTERRUPT_START ... EXIT_INTERRUPT_END:
            DEBUG_NOI("int %u", uop.extra - EXIT_INTERRUPT_START);
            break;
        default:
            DEBUG_NOI("<unknown %u>", uop.extra);
            break;
        }
        break;
    default:
        TODO("unhandled uop %d", uop.kind);
    }

    DEBUG("\n");
}

static void dump_uops(Vec(UOp) uops) {
    for_n(i, 1, vec_len(uops)) {
        debug_uop(uops, i, uops[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

// assume align is a power of two
static inline uintptr_t align_forward(uintptr_t ptr, uintptr_t align) {
    return (ptr + align - 1) & ~(align - 1);
}

Compiler compiler_new() {
    const usize initial_exec_size = 64*4096;

    Compiler c = {
        .manager = {
            .blocks = calloc(1, sizeof(*c.manager.blocks)),
            .exec_region = mmap(
                NULL, 
                initial_exec_size, 
                PROT_READ | PROT_WRITE | PROT_EXEC,  
                MAP_ANONYMOUS | MAP_PRIVATE, 
                -1, 
                0
            ),
            .exec_region_size = initial_exec_size,
            .exec_region_used = 0,
            .exec_region_freed = 0,
            .num_blocks_allocd = 0,
        },
        .cursor = 0,
        .uops = vec_new(UOp, 256),
        .code = vec_new(u8, 512),
        .sreg_status = {},
        .ip = {
            .offset_start = 0
        },
    };

    return c;
}

static void compiler_reset(Compiler* c) {
    BlockManager manager = c->manager;
    Vec(UOp) uops = c->uops;
    Vec(u8)  code = c->code;
    vec_clear(&uops);
    vec_clear(&code);
    *c = (Compiler){};
    c->manager = manager;
    c->uops = uops;
    c->code = code;
}

// Let's try out fibonacci hashing
static u64 paddr_hash(u64 paddr) {
    paddr = paddr >> 2;
    uint64_t x = paddr * 0x9e3779b97f4a7c15ULL;
    return x >> (64 - BLOCK_HASHMAP_CAPACITY_P2); 
}

CompiledBlock* compiler_compile_block(Compiler* c, EncodedInst* binary, u64 paddr) {

    compiler_reset(c);
    c->binary = binary;

    // insert a NOP so that index 0 can be used as a null value.
    vec_append(&c->uops, (UOp){.kind = UOP_NOP});
    // insert PARAM_LP and PARAM_TABLE so they can be taken up by hardcoded registers.
    vec_append(&c->uops, (UOp){.kind = UOP_PARAM_LP});
    vec_append(&c->uops, (UOp){.kind = UOP_PARAM_TABLE});

    // compile!
    DEBUG_INDENT;
    while (compile_next_inst(c)) {}
    dump_uops(c->uops);
    // x86-64 specific lmao
    x64_translate(c);
    DEBUG_DEDENT;

    // get a block in the executable region to do shit with
    u32 code_size = vec_len(c->code);
    if (c->manager.exec_region_used + code_size >= c->manager.exec_region_size) {
        TODO("reallocate!");
    }
    void* code = &c->manager.exec_region[c->manager.exec_region_used];
    c->manager.exec_region_used += code_size;
    memcpy(code, c->code, code_size);
    
    u64 phash = paddr_hash(paddr);
    CompiledBlock* block = &(*c->manager.blocks)[phash];
    if (block->code != nullptr) {
        // we gotta evict the block here. hopefully, this 
        // happens rarely if we have a good hash function.
        compiler_free_block(c, block);
    }
    c->manager.num_blocks_allocd += 1;
    block->code = code;
    block->code_size = code_size;
    return block;
}


CompiledBlock* compiler_retrieve_block(Compiler* c, u64 paddr) {
    u64 phash = paddr_hash(paddr);
    CompiledBlock* block = &(*c->manager.blocks)[phash];
    if (block->code == nullptr) {
        return nullptr;
    }
    return block;
}

void compiler_free_block(Compiler* c, CompiledBlock* block) {
    c->manager.exec_region_freed += block->code_size;
    c->manager.num_blocks_allocd -= 1;
    block->code = nullptr;
    block->code_size = 0;
}

void compiler_free_in_range(Compiler* c, u64 paddr_start, u64 paddr_end) {
    for (u64 paddr = paddr_start; paddr < paddr_end; paddr += 4) {
        u64 phash = paddr_hash(paddr);
        CompiledBlock* block = &(*c->manager.blocks)[phash];
        if (block->code != 0) {
            compiler_free_block(c, block);
        }
    }
}

void compiler_free_all(Compiler* c) {
    memset(c->manager.blocks, 0, sizeof(*c->manager.blocks));
    c->manager.num_blocks_allocd = 0;
    c->manager.exec_region_freed = 0;
    c->manager.exec_region_used = 0;
}
