#include "system.h"
#include "aphelion.h"
#include "common/portability.h"
#include "common/util.h"
#include "common/ansi.h"

#if defined(DEBUG_DEFINE)
    int global_debug_indent;
#endif

/// Initialize the emulated system.
System* system_init(u16 num_lps, u16 num_ram_slots, usize* ram_slot_sizes) {
    System* sys = malloc(sizeof(*sys));

    sys->lps_len = num_lps;
    sys->lps = malloc(sizeof(Lp) * num_lps);
    memset(sys->lps, 0, sizeof(Lp) * num_lps);

    // initialize each lp
    for_n(i, 0, num_lps) {
        Lp* lp = &sys->lps[i];
        lp->ctrl[CTRL_LPID] = i;

        lp->sys = sys;

        lp->compiler = compiler_new();
    }

    // init devices
    sys->bus.devices = vec_new(BusDevice, 64);

    // init ram
    sys->bus.ram_slots_len = num_ram_slots;
    sys->bus.ram_slots = malloc(sizeof(RamSlot) * num_ram_slots);
    for_n(i, 0, num_ram_slots) {
        RamSlot* slot = &sys->bus.ram_slots[i];
        slot->size_in_pages = ram_slot_sizes[i];
        slot->raw_memory = malloc(slot->size_in_pages);
    }

    return sys;
}

void system_dump(System* sys) {
    
    DEBUG("mode: ");
    switch (sys->mode) {
    case SYS_MODE_STANDARD: 
        DEBUG_NOI("standard\n");
        break;
    case SYS_MODE_SANDBOX: 
        DEBUG_NOI("sandbox\n");
        break;
    }
    DEBUG("ram:\n");
    DEBUG_INDENT;
    for_n(i, 0, sys->bus.ram_slots_len) {
        u64 start_addr = i * RAM_SLOT_MAX_SIZE;
        u64 slot_size = sys->bus.ram_slots[i].size_in_pages * APHEL_PAGE_SIZE;
        DEBUG("0x%016zx to 0x%016zx (%lu bytes)\n", 
            start_addr,
            start_addr + slot_size - 1,
            slot_size
        );
    }
    DEBUG_DEDENT;
    for_n(i, 0, sys->lps_len) {
        Lp* lp = &sys->lps[i];
        DEBUG("lp %zu: \n", i);
        DEBUG_INDENT;
        lp_dump_info(lp);
        DEBUG_DEDENT;
    }
}

static void quit_with_code SYSV_ABI (Lp* lp, const JitHelperTable* table, BlockExitCode code) {
    longjmp(lp->interrupt_state.interrupt_handler, code);
}

static const JitHelperTable table = {
    .quit_with_code = quit_with_code,
};


void system_launch(System* sys) {
    assert(sys->lps_len == 1);

    Lp* lp0 = &sys->lps[0];

    // set this LP's return point.
    int retcode = 0;
    if ((retcode = _setjmp(lp0->interrupt_state.interrupt_handler))) {
        printf("exited! with code %u\n", retcode);
        system_dump(sys);
        return;
    }

    u64 paddr = lp_translate_addr(lp0, lp0->gpr[GPR_IP], ACCESS_X);
    lp_check_physical_access(lp0, paddr, WIDTH_32, ACCESS_X);
    void* haddr = lp_physical_get_haddr(lp0, paddr);

    CompiledBlock* block = compiler_compile_block(&lp0->compiler, haddr, paddr);
    
    block->code(lp0, &table);
}
