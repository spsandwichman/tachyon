#include "system.h"
#include "aphelion.h"
#include "common/portability.h"
#include "common/util.h"

/// Initialize the emulated system.
System* system_init(u16 num_lps, u16 num_ram_slots, u32* ram_slot_sizes) {
    System* sys = malloc(sizeof(*sys));

    sys->lps_len = num_lps;
    sys->lps = malloc(sizeof(Lp) * num_lps);
    memset(sys->lps, 0, sizeof(Lp) * num_lps);

    // initialize each lp
    for_n(i, 0, num_lps) {
        Lp* lp = &sys->lps[i];
        lp->ctrl[CTRL_ID] = i;

        // initialize translation cache
        // lp->tc;
        lp->sys = sys;
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

/// Verify the validity of an access before it is executed.
/// Returns if the access passed safety checks, diverges and 
/// triggers relevant interrupts if access would definitely fail.
void bus_verify_access(Lp* lp, u64 address, AccessWidth width, AccessKind kind) {
    // check unaligned
    if ((address & ((1 << width) - 1)) != 0) {
        lp_trigger_interrupt(lp, ICAUSE_UALIGNR + kind);
    }

    System* sys = lp->sys;

    if (address <= BUS_RAM_MAX) {
        u16 ram_slot_index = (address >> RAM_SLOT_MAX_SIZE_BITS) & (MAX_RAM_SLOTS - 1);
        if (ram_slot_index >= sys->bus.ram_slots_len) {
            lp_trigger_interrupt(lp, ICAUSE_BUSR + kind);
        }

        RamSlot slot = sys->bus.ram_slots[ram_slot_index];

        u64 addr_inside_slot = address & (RAM_SLOT_MAX_SIZE - 1);

        if (addr_inside_slot >= slot.size_in_pages * PAGE_SIZE) {
            lp_trigger_interrupt(lp, ICAUSE_BUSR + kind);
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
void bus_raw_write(Lp* lp, u64 address, AccessWidth width, void* data) {
    System* sys = lp->sys;

    if_likely (address <= BUS_RAM_MAX) {
        // writes to RAM are like. infinitely more likely than writes 
        // to any other part of the address space.

        u16 ram_slot_index = (address >> RAM_SLOT_MAX_SIZE_BITS) & (MAX_RAM_SLOTS - 1);
        u64 addr_inside_slot = address & (RAM_SLOT_MAX_SIZE - 1);

        RamSlot slot = sys->bus.ram_slots[ram_slot_index];
        void* haddr = &((u8*)slot.raw_memory)[addr_inside_slot];
        
        ASSUME(width <= WIDTH_MAX);
        
        // which one is better? memcpy or switch? ill do memcpy for now
        memcpy(haddr, data, 1 << width);
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
        if ((other_lp->llsc_lock.addr & width_mask) != (address & width_mask)) {
            continue;
        }

        other_lp->llsc_lock.locked = false;
    }
}

