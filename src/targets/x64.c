#include "x64.h"
#include "chasm/asm_x64.h"
#include "common/util.h"
#include "common/vec.h"

static void predefine_hreg(UOpIndex definition_of[X64_REG__COUNT], X64_Reg hreg_of[], UOpIndex def, X64_Reg hreg) {
    definition_of[hreg] = def;
    hreg_of[def] = hreg;
}

static X64_Reg new_hreg(UOpIndex definition_of[X64_REG__COUNT], X64_Reg hreg_of[], UOpIndex def) {
    for_n(i, 0, X64_REG__COUNT) {
        switch (i) {
        case X64_REG_RBP:
        case X64_REG_RSP:
            continue;
        }

        if (definition_of[i] == UOP_INDEX_NULL) {
            definition_of[i] = def;
            hreg_of[def] = i;
            return i;
        }
    }
    TODO("spill to stack?");
}

/// Get the real register defined by a specific uop, or allocate one if necessary.
static X64_Reg get_or_allocate_hreg(UOpIndex definition_of[X64_REG__COUNT], X64_Reg hreg_of[], UOpIndex def) {
    for_n(hreg, 0, X64_REG__COUNT) {
        if (definition_of[hreg] == def) {
            return hreg;
        }
    }
    return new_hreg(definition_of, hreg_of, def);
}

static X64_Reg free_hreg(UOpIndex definition_of[X64_REG__COUNT], X64_Reg hreg_of[], UOpIndex def) {
    for_n(hreg, 0, X64_REG__COUNT) {
        if (definition_of[hreg] == def) {
            definition_of[hreg] = UOP_INDEX_NULL;
            return hreg;
        }
    }
    // weird, must be an unused result. get some unused location to throw it away into,
    // the immediately free it again.
    X64_Reg reg = get_or_allocate_hreg(definition_of, hreg_of, def);
    definition_of[reg] = UOP_INDEX_NULL;
    return reg;
}

const x64Operand chasm_reg[X64_REG__COUNT] = {
    [X64_REG_RAX] = rax,
    [X64_REG_RBX] = rbx,
    [X64_REG_RCX] = rcx,
    [X64_REG_RDX] = rdx,
    [X64_REG_RSI] = rsi,
    [X64_REG_RDI] = rdi,
    [X64_REG_RBP] = rbp,
    [X64_REG_RSP] = rsp,
    [X64_REG_R8]  = r8,
    [X64_REG_R9]  = r9,
    [X64_REG_R10] = r10,
    [X64_REG_R11] = r11,
    [X64_REG_R12] = r12,
    [X64_REG_R13] = r13,
    [X64_REG_R14] = r14,
    [X64_REG_R15] = r15,
};
const x64Operand chasm_reg32[X64_REG__COUNT] = {
    [X64_REG_RAX] = eax,
    [X64_REG_RBX] = ebx,
    [X64_REG_RCX] = ecx,
    [X64_REG_RDX] = edx,
    [X64_REG_RSI] = esi,
    [X64_REG_RDI] = edi,
    [X64_REG_RBP] = ebp,
    [X64_REG_RSP] = esp,
    [X64_REG_R8]  = r8d,
    [X64_REG_R9]  = r9d,
    [X64_REG_R10] = r10d,
    [X64_REG_R11] = r11d,
    [X64_REG_R12] = r12d,
    [X64_REG_R13] = r13d,
    [X64_REG_R14] = r14d,
    [X64_REG_R15] = r15d,
};

const enum x64RegisterReference chasm_ref[X64_REG__COUNT] = {
    [X64_REG_RAX] = $rax,
    [X64_REG_RBX] = $rbx,
    [X64_REG_RCX] = $rcx,
    [X64_REG_RDX] = $rdx,
    [X64_REG_RSI] = $rsi,
    [X64_REG_RDI] = $rdi,
    [X64_REG_RBP] = $rbp,
    [X64_REG_RSP] = $rsp,
    [X64_REG_R8]  = $r8,
    [X64_REG_R9]  = $r9,
    [X64_REG_R10] = $r10,
    [X64_REG_R11] = $r11,
    [X64_REG_R12] = $r12,
    [X64_REG_R13] = $r13,
    [X64_REG_R14] = $r14,
    [X64_REG_R15] = $r15,
};

void x64_translate(Compiler* c) {
    Vec(x64Ins) insts = vec_new(x64Ins, 256);

    X64_Reg hreg_of[vec_len(c->uops)];
    memset(hreg_of, X64_REG__COUNT, vec_len(c->uops));
    UOpIndex definition_of[X64_REG__COUNT] = {};

    const UOpIndex param_lp = 1;
    const UOpIndex param_table = 2;
    predefine_hreg(definition_of, hreg_of, param_lp, X64_REG_RDI);
    predefine_hreg(definition_of, hreg_of, param_table, X64_REG_RSI);

    for(UOpIndex op_index = vec_len(c->uops) - 1; op_index > 0; op_index--) {
        UOp op = c->uops[op_index];
        switch (op.kind) {
        case UOP_NOP:
        case UOP_PARAM_LP:
        case UOP_PARAM_TABLE:
            break;
        case UOP_EXIT:
            break;
        case UOP_GET_SREG:
            break;
        case UOP_PUT_SREG: {
            X64_Reg value = get_or_allocate_hreg(definition_of, hreg_of, op.src1);
            break;
        }
        case UOP_ADD: {
            X64_Reg def = free_hreg(definition_of, hreg_of, op_index);
            X64_Reg src1 = get_or_allocate_hreg(definition_of, hreg_of, op.src1);
            X64_Reg src2 = get_or_allocate_hreg(definition_of, hreg_of, op.src2);
            break;
        }
        case UOP_SUB: {
            X64_Reg def = free_hreg(definition_of, hreg_of, op_index);
            X64_Reg src1 = get_or_allocate_hreg(definition_of, hreg_of, op.src1);
            X64_Reg src2 = get_or_allocate_hreg(definition_of, hreg_of, op.src2);
            break;
        } 
        case UOP_MUL: {
            X64_Reg def = free_hreg(definition_of, hreg_of, op_index);
            X64_Reg src1 = get_or_allocate_hreg(definition_of, hreg_of, op.src1);
            X64_Reg src2 = get_or_allocate_hreg(definition_of, hreg_of, op.src2);
            break;
        }
        case UOP_ADD_I32:
        case UOP_SUB_I32: {
            X64_Reg def = free_hreg(definition_of, hreg_of, op_index);
            X64_Reg src1 = get_or_allocate_hreg(definition_of, hreg_of, op.src1);
            break;
        }
        case UOP_SET_I32: {
            X64_Reg def = free_hreg(definition_of, hreg_of, op_index);
            break;
        }
        default:
            TODO("unhandled uop %u", op.kind);
        }
    }

    for(UOpIndex op_index = 1; op_index < vec_len(c->uops); op_index++) {
        UOp op = c->uops[op_index];
        switch (op.kind) {
        case UOP_NOP:
        case UOP_PARAM_LP:
        case UOP_PARAM_TABLE:
            break;
        case UOP_EXIT:
            vec_append(&insts,((x64Ins){ 
                MOV, {
                    rdx, 
                    imm(op.extra)} })
            );
            vec_append(&insts, ((x64Ins){ 
                CALL, {m64(
                    chasm_ref[hreg_of[param_table]], 
                    offsetof(JitHelperTable, exit))}})
            );
            break;
        case UOP_GET_SREG: {
            AphelGpr sreg = op.extra;
            vec_append(&insts, ((x64Ins){ 
                MOV, {
                    chasm_reg[hreg_of[op_index]], 
                    m64(chasm_ref[hreg_of[param_lp]], offsetof(Lp, gpr[sreg]))}})
            );
            break;
        }
        case UOP_PUT_SREG: {
            AphelGpr sreg = op.extra;
            vec_append(&insts, ((x64Ins){
                MOV, {
                    m64(chasm_ref[hreg_of[param_lp]], offsetof(Lp, gpr[sreg])), 
                    chasm_reg[hreg_of[op.src1]]}})
            );
            break;
        }
        case UOP_ADD: {
            if (hreg_of[op_index] != hreg_of[op.src1]) {
                vec_append(&insts, ((x64Ins){
                    MOV, {
                        chasm_reg[hreg_of[op_index]], 
                        chasm_reg[hreg_of[op.src1]]}})
                );
            }
            vec_append(&insts, ((x64Ins){
                ADD, {
                    chasm_reg[hreg_of[op_index]], 
                    chasm_reg[hreg_of[op.src2]]}})
            );
            break;
        }
        case UOP_MUL: {
            if (hreg_of[op_index] != hreg_of[op.src1]) {
                vec_append(&insts, ((x64Ins){
                    MOV, {
                        chasm_reg[hreg_of[op_index]], 
                        chasm_reg[hreg_of[op.src1]]}})
                );
            }
            vec_append(&insts, ((x64Ins){
                IMUL, {
                    chasm_reg[hreg_of[op_index]], 
                    chasm_reg[hreg_of[op.src2]]}})
            );
            break;
        }
        case UOP_SUB: {
            if (hreg_of[op_index] != hreg_of[op.src1]) {
                vec_append(&insts, ((x64Ins){
                    MOV, {
                        chasm_reg[hreg_of[op_index]], 
                        chasm_reg[hreg_of[op.src1]]}})
                );
            }
            vec_append(&insts, ((x64Ins){
                SUB, {
                    chasm_reg[hreg_of[op_index]], 
                    chasm_reg[hreg_of[op.src2]]}})
            );
            break;
        }
        case UOP_SET_I32: {
            vec_append(&insts, ((x64Ins){
                MOV, {
                    chasm_reg32[hreg_of[op_index]], 
                    im32(op.imm32)}})
            );
            break;
        }
        case UOP_ADD_I32: {
            if (hreg_of[op_index] != hreg_of[op.src1]) {
                vec_append(&insts, ((x64Ins){
                    MOV, {
                        chasm_reg[hreg_of[op_index]], 
                        chasm_reg[hreg_of[op.src1]]}})
                );
            }
            vec_append(&insts, ((x64Ins){
                ADD, {
                    chasm_reg[hreg_of[op_index]], 
                    imm(op.imm32)}})
            );
            break;
        }
        
        case UOP_SUB_I32: {
            if (hreg_of[op_index] != hreg_of[op.src1]) {
                vec_append(&insts, ((x64Ins){
                    MOV, {
                        chasm_reg[hreg_of[op_index]], 
                        chasm_reg[hreg_of[op.src1]]}})
                );
            }
            vec_append(&insts, ((x64Ins){
                SUB, {
                    chasm_reg[hreg_of[op_index]], 
                    imm(op.imm32)}})
            );
            break;
        }
        default:
            TODO("unhandled uop %u", op.kind);
        }
    }

    DEBUG_NOI("\n");
    #ifdef DEBUG_DEFINE
    for_n(i, 0, vec_len(insts)) {
        char* inst_str = x64stringify(&insts[i], 1);
        DEBUG("%s\n", inst_str);
        free(inst_str);
    }
    #endif

    DEBUG_NOI("\n");
    u8 buffer[16] = {};
    for_n(i, 0, vec_len(insts)) {
        u32 len = x64emit(&insts[i], buffer);
        if (len == 0) {
            TODO("error: %s\n", x64error(nullptr));
        }
        vec_reserve(&c->code, len);
        memcpy(&c->code[vec_len(c->code)], buffer, len);
        vec_len(c->code) += len;
        DEBUG("");
        for_n(j, 0, len) {
            DEBUG_NOI("%02x ", buffer[j]);
        }
        DEBUG_NOI("\n");
    }
}
