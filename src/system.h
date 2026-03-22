#ifndef TACHYON_SYSTEM_H
#define TACHYON_SYSTEM_H

#include "aphelion.h"
#include "common/portability.h"
#include "common/type.h"
#include "common/vec.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

#define KiB (1024ull)
#define MiB (1024ull * 1024ull)
#define GiB (1024ull * 1024ull * 1024ull)

#if defined(__clang__)
    #define PRESERVE_ALL  [[clang::preserve_all]]
    #define PRESERVE_MOST [[clang::preserve_most]]
    #define ASSUME(x)     __builtin_assume(x)
#elif defined(__GNUC__)
    #define PRESERVE_ALL  [[preserve_all]]
    #define PRESERVE_MOST [[preserve_most]]
    #define ASSUME(x)     if (!(x)) __builtin_unreachable()
#elif
    #error "Must use Clang or GCC."
#endif

#define DBG

#if defined(DBG)
    #define DEBUG(msg, ...) do {\
        printf(msg __VA_OPT__(,) __VA_ARGS__);\
        fflush(stdout);\
    } while (0)
#else
    #define DEBUG(...)
#endif


#define TC_SIZE 256

#define MAX_LPS 512

typedef struct System System;
typedef struct BusDevice BusDevice;

typedef struct PteEntry {
    u64 v : 1;
    u64 w : 1;
    u64 x : 1;
    u64 unused : 9;
    u64 next   : 52;
} PteEntry;
static_assert(sizeof(PteEntry) == sizeof(u64));

/// Translation cache entry
typedef struct TcEntry {
    u64 virt_page_indices: 36;
    bool user : 1;
    bool present : 1;
    u16 ptp_hash; // maybe this is dumb? lets see
    union {
        u64 entry_bits;
        PteEntry entry;
    };
} TcEntry;

/// Translation cache
typedef struct LpTc {
    TcEntry entries[TC_SIZE];
} LpTc;

typedef struct LpLockState {
    u64 addr;
    u8 width;
    bool locked;
} LpLockState;

typedef struct LpInterruptState {
    jmp_buf interrupt_handler;
} LpInterruptState;

typedef struct Lp {
    u64 gpr[GPR__COUNT];
    u64 ctrl[CTRL__COUNT];

    System* sys;

    LpTc tc;
    LpLockState llsc_lock;
    LpInterruptState interrupt_state;
} Lp;

#define RAM_SLOT_MAX_SIZE_BITS 38 
#define RAM_SLOT_MAX_SIZE (1ull << RAM_SLOT_MAX_SIZE_BITS)
#define MAX_RAM_SLOTS 512
#define PAGE_SIZE 4096

typedef struct RamSlot {
    void* raw_memory;
    u64 size_in_pages;
} RamSlot;

#define BUS_RAM_MAX 0x00007FFFFFFFFFFF
#define BUS_DEV_MAX 0xFFFFFFFEFFFFFFFF
#define BUS_SRR_MAX 0xFFFFFFFFFFFFFFFF

/// Handles system-wide physical memory accesses
typedef struct SystemBus {
    Vec(BusDevice) devices;
    RamSlot* ram_slots;
    u16 ram_slots_len;
} SystemBus;

typedef enum: u8 {
    WIDTH_8  = 0,
    WIDTH_16 = 1,
    WIDTH_32 = 2,
    WIDTH_64 = 3,

    WIDTH_MAX = WIDTH_64,

} AccessWidth;

typedef enum: u8 {
    ACCESS_R = 0,
    ACCESS_W = 1,
    ACCESS_X = 2,
} AccessKind;

struct BusDevice {
    u64 range_start;
    u64 range_end;

    // A LP wants to read from a physical address.
    void (*respond_read_rq)(Lp* lp, u64 address, AccessWidth width, void* data);
    // A LP wants to fetch an instruction from a physical address.
    void (*respond_fetch_rq)(Lp* lp, u32* data);
    // A LP wants to write from a physical address.
    void (*respond_write_rq)(Lp* lp, u64 address, AccessWidth width, void* data);
};

typedef struct SystemFlags {
    bool mode_test;
} SystemFlags;

struct System {
    Lp* lps;

    u16 lps_len;

    SystemBus bus;

    SystemFlags flags;
};

System* system_init(u16 num_lps, u16 num_ram_slots, usize* ram_slot_sizes);

// Trigger an interrupt and longjmp back to the main execution loop.
NORETURN void lp_trigger_interrupt(Lp* lp, u8 cause);
// Dump the lp's state to stdout.
void lp_dump_info(Lp* lp);

typedef union EncodedInst {
    u8 opcode;
    struct {
        u32 opcode : 8;
        u32 r1 : 5;
        u32 imm : 19;
    } A;
    struct {
        u32 opcode : 8;
        u32 r1 : 5;
        u32 r2 : 5;
        u32 imm : 14;
    } B;
    struct {
        u32 opcode : 8;
        u32 r1 : 5;
        u32 r2 : 5;
        u32 r3 : 5;
        u32 imm : 9;
    } C;
} EncodedInst;

#endif // TACHYON_CORE_H
