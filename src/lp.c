#include "common/portability.h"
#include "common/util.h"
#include "aphelion.h"
#include "system.h"

#include <assert.h>

// noreturn void lp_trigger_interrupt(Lp *lp, u8 cause) {

//     DEBUG("interrupt on LP %lu with code %u\n", lp->ctrl[CTRL_LPID], cause);

//     assert(cause < ICAUSE__COUNT);

//     // save ip and stat to intip and intstat
//     lp->ctrl[CTRL_INTIP] = lp->gpr[GPR_IP];
//     lp->ctrl[CTRL_INTSTAT] = lp->ctrl[CTRL_STAT];

//     // set ip to corresponding handler
//     lp->gpr[GPR_IP] = lp->ctrl[CTRL_INT0 + cause];

//     // set intcause
//     lp->ctrl[CTRL_INTCAUSE] = cause;

//     // disable external interrupts, enter kernel mode
//     lp->ctrl[CTRL_STAT] &= ~0b11ull;

//     // unlock lock state
//     lp->llsc_lock.locked = false;

//     // longjmp(
//     //     lp->interrupt_state.interrupt_handler, 
//     //     EXIT_INTERRUPT_START + cause
//     // );
//     TODO();
// }

void lp_dump(Lp* lp) {
    DEBUG("");
    for_n(i, 0, GPR__COUNT) {
        DEBUG_NOI("  %3s = %016lx", gpr_name[i], lp->gpr[i]);

        if (i % 4 == 3) {
            DEBUG_NOI("\n");
            DEBUG("");
        }
    }
    DEBUG("\n");
    
    DEBUG("");
    for_n(i, 0, CTRL__COUNT) {
        DEBUG_NOI(" %8s = %016lx", ctrl_name[i], lp->ctrl[i]);

        if (i % 4 == 3) {
            DEBUG_NOI("\n");
            DEBUG("");
        }
    }
    DEBUG("\n");

    DEBUG("locked %d addr %016lx width %d\n", 
        lp->llsc_lock.locked, 
        lp->llsc_lock.addr,
        1 << lp->llsc_lock.width
    );

    DEBUG("compiler\n");
    DEBUG_INDENT;
    DEBUG("live blocks %8u / %-8u (%.2f %%)\n", 
        lp->compiler.manager.num_blocks_allocd,
        BLOCK_HASHMAP_CAPACITY,
        (float)lp->compiler.manager.num_blocks_allocd/BLOCK_HASHMAP_CAPACITY
    );

    u32 actually_used = lp->compiler.manager.exec_region_used - lp->compiler.manager.exec_region_freed;
    DEBUG("exec used   %8u / %-8u (%.2f %%)\n", 
        actually_used,
        lp->compiler.manager.exec_region_size,
        (float)actually_used/lp->compiler.manager.exec_region_size
    );
    DEBUG("exec wasted %8u / %-8u (%.2f %%)\n", 
        lp->compiler.manager.exec_region_freed,
        lp->compiler.manager.exec_region_size,
        (float)lp->compiler.manager.exec_region_freed/lp->compiler.manager.exec_region_size
    );
    DEBUG_DEDENT;
}

/// Perform a TLB lookup.
/// \return True if lookup succeedeed, false otherwise.
bool tlb_lookup(Lp* lp, u64 addr_in, PteEntry* addr_out) {

    LpTlb* tlb = &lp->tlb;
    
    u64 page_indices = ((addr_in << 16) >> 28);

    for_n(i, 0, TLB_SIZE) {
        TlbEntry ent = tlb->entries[i];
        if (!ent.global && ent.asid == lp->ctrl[CTRL_ASID]) {

        }
    }

    return false;
}

#define GETBITS(x, low, high) (((u64)x >> low) & ((1ull << (high-low + 1)) - 1))

/// Returns to a corresponding `setjmp` on translation failure.
JIT_HELPER GENERAL_REGS_ONLY
u64 jit_translate_vaddr(Lp* lp, const JitHelperTable* table, u64 vaddr, AccessKind kind) {
    u64 vr = vaddr >> 62;
    // ASSUME(vr < 4);

    u64 l3pt;
    switch (vr) {
    case 0: // low paged region
        if (GETBITS(vaddr, 47, 61) != 0) {
            table_trigger_interrupt(lp, table, ICAUSE_ACCESSR + kind);
        }
        l3pt = lp->ctrl[CTRL_LPTP];
        break;
    case 1:
    case 2: // direct regions 
        if (lp->ctrl[CTRL_STAT] & STAT_BIT_U) {
            table_trigger_interrupt(lp, table, ICAUSE_ACCESSR + kind);
        }
        return (vaddr << 2) >> 2;
    case 3: // high paged region
        if (((i64)vaddr >> 47) != -1) {
            table_trigger_interrupt(lp, table, ICAUSE_ACCESSR + kind);
        }
        l3pt = lp->ctrl[CTRL_HPTP];
        break;
    }

    TODO("guh");
}

/// Get the host address associated with a physical address inside a RAM region.
/// Assumes paddr is valid.
u8* lp_physical_get_haddr(Lp* lp, u64 paddr) {
    System* sys = lp->sys;
    u16 ram_slot_index = (paddr >> RAM_SLOT_MAX_SIZE_BITS) & (MAX_RAM_SLOTS - 1);
    u64 addr_inside_slot = paddr & (RAM_SLOT_MAX_SIZE - 1);
    RamSlot slot = sys->bus.ram_slots[ram_slot_index];
    return &(slot.raw_memory)[addr_inside_slot];
}

/// Verify the validity of an access before it is executed.
/// Returns if the access passed safety checks, diverges and 
/// triggers relevant interrupts if access would definitely fail.
JIT_HELPER GENERAL_REGS_ONLY
void jit_verify_access(Lp* lp, const JitHelperTable* table, u64 paddr, AccessWidth width, AccessKind kind) {
    // check unaligned
    if ((paddr & ((1 << width) - 1)) != 0) {
        table_trigger_interrupt(lp, table, ICAUSE_UALIGNR + kind);
    }

    System* sys = lp->sys;

    if (paddr <= BUS_RAM_MAX) {
        u16 ram_slot_index = (paddr >> RAM_SLOT_MAX_SIZE_BITS) & (MAX_RAM_SLOTS - 1);
        if (ram_slot_index >= sys->bus.ram_slots_len) {
            table_trigger_interrupt(lp, table, ICAUSE_BUSR + kind);
        }

        RamSlot slot = sys->bus.ram_slots[ram_slot_index];

        u64 addr_inside_slot = paddr & (RAM_SLOT_MAX_SIZE - 1);

        if (addr_inside_slot >= slot.size_in_pages * APHEL_PAGE_SIZE) {
            table_trigger_interrupt(lp, table, ICAUSE_BUSR + kind);
        }

        // verified!
        return;
    } else {
        // this is outside of RAM, so we can't check its validity in advance.
        // its safety must be checked when the access is actually executed.

        // TODO when the layout of the SRR is solidified,
        // we can move some checking into this function.
        return;
    }
}

/// Perform a bus write, performing as little safety checks as possible.
/// Assumes `bus_verify` has executed successfully beforehand.
JIT_HELPER GENERAL_REGS_ONLY
void jit_raw_write(Lp* lp, const JitHelperTable* table, u64 paddr, AccessWidth width, u64 data) {
    System* sys = lp->sys;

    if_likely (paddr <= BUS_RAM_MAX) {
        // writes to RAM are like. infinitely more likely than writes 
        // to any other part of the address space.

        u16 ram_slot_index = (paddr >> RAM_SLOT_MAX_SIZE_BITS) & (MAX_RAM_SLOTS - 1);
        u64 addr_inside_slot = paddr & (RAM_SLOT_MAX_SIZE - 1);

        RamSlot slot = sys->bus.ram_slots[ram_slot_index];
        void* haddr = &(slot.raw_memory)[addr_inside_slot];
        
        // hopefully this can optimize the memcpy a little bit
        ASSUME(width <= WIDTH_MAX);

        memcpy(haddr, &data, 1 << width);
    } else {
        TODO("non-ram write");
    }

    // invalidate lock states
    for_n(i, 0, sys->lps_len) {
        Lp* other_lp = &sys->lps[i];
        if (!other_lp->llsc_lock.locked) {
            continue;
        }

        // check rough lock range overlap
        const u64 width_mask = ~((1ull << WIDTH_MAX) - 1);
        if ((other_lp->llsc_lock.addr & width_mask) != (paddr & width_mask)) {
            continue;
        }

        other_lp->llsc_lock.locked = false;
    }
}

JIT_HELPER GENERAL_REGS_ONLY
u64 jit_raw_read(Lp* lp, const JitHelperTable* table, u64 paddr, AccessWidth width) {
    System* sys = lp->sys;

    u64 retval;

    if_likely (paddr <= BUS_RAM_MAX) {
        u16 ram_slot_index = (paddr >> RAM_SLOT_MAX_SIZE_BITS) & (MAX_RAM_SLOTS - 1);
        u64 addr_inside_slot = paddr & (RAM_SLOT_MAX_SIZE - 1);

        RamSlot slot = sys->bus.ram_slots[ram_slot_index];
        void* haddr = &(slot.raw_memory)[addr_inside_slot];
        
        // hopefully this can optimize the memcpy a little bit
        ASSUME(width <= WIDTH_MAX);

        switch (width) {
        case WIDTH_8:  retval =  *(u8*)haddr; break;
        case WIDTH_16: retval = *(u16*)haddr; break;
        case WIDTH_32: retval = *(u32*)haddr; break;
        case WIDTH_64: retval = *(u64*)haddr; break;
        default: unreachable();
        }
    } else {
        TODO("non-ram read");
    }

    return retval;
}

SYSV_ABI noreturn
void jit_exit(Lp* lp, const JitHelperTable* table, BlockExitCode code) {
    longjmp(lp->interrupt_state.interrupt_handler, code);
}

static const JitHelperTable table = {
    .exit = jit_exit,
    .translate_vaddr = jit_translate_vaddr,
    .verify_access = jit_verify_access,
    .raw_write = jit_raw_write,
    .raw_read = jit_raw_read,
};

void lp_dispatch(Lp* lp) {
    
    // set this LP's return point.
    int retcode = 0;
    if ((retcode = _setjmp(lp->interrupt_state.interrupt_handler))) {
        DEBUG("exited! with code %u\n", retcode);
        system_dump(lp->sys);
        return;
    }

    u64 paddr = jit_translate_vaddr(lp, &table, lp->gpr[GPR_IP], ACCESS_X);
    jit_verify_access(lp, &table, paddr, WIDTH_32, ACCESS_X);
    void* haddr = lp_physical_get_haddr(lp, paddr);

    CompiledBlock* block = compiler_compile_block(&lp->compiler, haddr, paddr);
    
    block->code(lp, &table);
}