#ifndef COMPILER_H
#define COMPILER_H

#include "aphelion.h"
#include "system.h"
#include <assert.h>

/*
    This is Tachyon's heart, the basic-block JIT compiler. 
    
    It takes a continuous stream of non-control-flow Aphelion instructions 
    and compiles them to a list of basic operations called `UOp`s.
    These `UOp`s are then simply patched together.

    Crucially, the `UOp` list may be optimized.

    For example, emulated registers do not always have to be loaded from/
    stored to the LP's register bank after reach instruction. We may keep them 
    in a limited set of host registers for a surprisingly long time, and only 
    write back when strictly required.

    For a more complex example, a register holding a pointer that is used 
    may times in a block only has to be translated once for each kind of 
    access, unless the page table/TLB is explicitly invalidated. Even with 
    a TLB, this should drastically reduce the time spent in the memory system.

    Optionally, several technically-unsafe optimizations may be enabled.
    For example, it is possible to tell the optimizer to assume that 
    translation works such that T(vaddr + const) = T(vaddr) + const, 
    allowing the optimizer to reduce vaddr translation calls by a large factor,
    especially for struct/array accesses. This assumption is correct in almost 
    all cases, but is unable to catch edge cases near page boundaries.

    A brief glossary:

    vaddr, virtual address  a virtual address on the emulated machine.
    paddr, phys address     an address on the emulated machine.
    haddr, host address     an address on the host machine.

    sreg, system register   a register on the emulated machine: a0, a1, etc.
    hreg, host register     a register on the host machine: rax, rbx, etc.
*/

///
typedef enum : u8 {
    UOP_NOP,

    UOP_GET_SREG,
    UOP_PUT_SREG,

    UOP_SET_I32,
    UOP_SET_U32,

    /// Translate a vaddr to a paddr.
    UOP_VADDR_TRANSLATE,

    /// Perform the verification stage of a paddr bus access.
    /// This is a separate UOp so that it may be removed in optimization.
    UOP_BUS_VERIFY,

    /// Read from a paddr, assuming UOP_BUS_VERIFY 
    /// has been executed at some point before.
    UOP_BUS_RAW_READ_64,
    UOP_BUS_RAW_READ_32,
    UOP_BUS_RAW_READ_16,
    UOP_BUS_RAW_READ_8,

    /// Write to a paddr, assuming UOP_BUS_VERIFY 
    /// has been executed at some point before.
    UOP_BUS_RAW_WRITE_64,
    UOP_BUS_RAW_WRITE_32,
    UOP_BUS_RAW_WRITE_16,
    UOP_BUS_RAW_WRITE_8,

    UOP_ADD,
    UOP_SUB,
    UOP_MUL,
    UOP_UDIV,
    UOP_UDIV_UNCHECKED,
    UOP_IDIV,
    UOP_IDIV_UNCHECKED,

    UOP_BZ,
    UOP_BN,

    /// Unconditional branch.
    UOP_B,

    /// Check for pending external interrupts.
    /// If pending, trigger that interrupt and diverge.
    UOP_CHECK_PENDING,
    
    /// Exit with an exit code. It does not end the block.
    UOP_EXIT_MAY_RETURN,

    UOP_EXIT,
} UOpKind;

typedef enum {
    
    // execute the next block
    BLOCK_EXIT_NEXT = 1,

    // exit the test.
    BLOCK_EXIT_TEST_QUIT,

    BLOCK_EXIT_INTERRUPT_START,
    BLOCK_EXIT_INTERRUPT_END = BLOCK_EXIT_INTERRUPT_START + ICAUSE__COUNT,

} BlockExitCode;

#define UOP_INDEX_NULL 0
typedef u16 UOpIndex;

typedef struct UOp {
    UOpKind kind;
    u16 extra;
    UOpIndex src1;
    UOpIndex src2;
    u32 imm32;
} UOp;

typedef struct UOpBlock {

} UOpBlock;

UOpBlock* compile_block(System* sys, Lp* lp, EncodedInst* binary);

#endif // COMPILER_H
