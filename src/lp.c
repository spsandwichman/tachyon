#include "common/util.h"
#include "aphelion.h"
#include "system.h"
#include "compiler.h"

#include <setjmp.h>

NORETURN
void lp_trigger_interrupt(Lp *lp, u8 cause) {

    DEBUG("interrupt on LP %lu with code %u\n", lp->ctrl[CTRL_ID], cause);

    assert(cause < ICAUSE__COUNT);

    // save ip and stat to intip and intstat
    lp->ctrl[CTRL_INTIP] = lp->gpr[GPR_IP];
    lp->ctrl[CTRL_INTSTAT] = lp->ctrl[CTRL_STAT];

    // set ip to corresponding handler
    lp->gpr[GPR_IP] = lp->ctrl[CTRL_INT0 + cause];

    // set intcause
    lp->ctrl[CTRL_INTCAUSE] = cause;

    // disable external interrupts, enter kernel mode
    lp->ctrl[CTRL_STAT] &= ~0b11ull;

    // unlock lock state
    lp->llsc_lock.locked = false;

    longjmp(
        lp->interrupt_state.interrupt_handler, 
        BLOCK_EXIT_INTERRUPT_START + cause
    );
}

void lp_dump_info(Lp* lp) {
    printf("dump LP %lu (%p)\n", lp->ctrl[CTRL_ID], lp);

    for_n(i, 0, GPR__COUNT) {
        printf("  %3s = %016lx", gpr_name[i], lp->gpr[i]);

        if (i % 4 == 3) {
            printf("\n");
        }
    }
    printf("\n");
    
    for_n(i, 0, CTRL__COUNT) {
        printf(" %8s = %016lx", ctrl_name[i], lp->ctrl[i]);

        if (i % 4 == 3) {
            printf("\n");
        }
    }
    printf("\n");

    printf(" locked %d addr %016lx width %d\n", 
        lp->llsc_lock.locked, 
        lp->llsc_lock.addr,
        1 << lp->llsc_lock.width
    );
}

/// Perform a translation cache lookup.
/// \return True if lookup succeedeed, false otherwise.
bool tc_lookup(LpTc* tc, u64 addr_in, PteEntry* addr_out, u16 ptp_hash, bool user) {
    
    u64 page_indices = ((addr_in << 16) >> 28);

    for_n(i, 0, TC_SIZE) {
        TcEntry ent = tc->entries[i];

        if (ent.virt_page_indices != page_indices) {
            continue;
        }

        if (ent.user != user) {
            continue;
        }

        if (ent.ptp_hash != ptp_hash) {
            continue;
        }

        // translation cache entry hit
        *addr_out = ent.entry;
        return true;
    }

    return false;
}

/// Returns to a corresponding `setjmp` on translation failure.
u64 lp_translate_addr(Lp* lp, u64 vaddr, AccessKind kind) {
    
    PteEntry pte;

    bool user_mode = (lp->ctrl[CTRL_STAT] & (1 << 1)) != 0;

    u64 ptp = lp->ctrl[user_mode ? CTRL_UPTP : CTRL_KPTP];
    u16 ptp_hash = ptp ^ (ptp >> 16) ^ (ptp >> 32) ^ (ptp >> 48);

    // perform a TC lookup
    if (tc_lookup(&lp->tc, vaddr, &pte, ptp_hash, user_mode)) {
        if (!pte.v) {
            lp_trigger_interrupt(lp, ICAUSE_ACCESSR + kind);
        }

        if (!pte.w && kind == ACCESS_W) {
            lp_trigger_interrupt(lp, ICAUSE_ACCESSW);
        }

        if (!pte.x && kind == ACCESS_X) {
            lp_trigger_interrupt(lp, ICAUSE_ACCESSX);
        }
        
        u64 paddr = (pte.next << 12) | (vaddr & ((1 << 12) - 1));
        return paddr;
    }

    // perform a page table walk
    TODO("perform page table walk");
}
