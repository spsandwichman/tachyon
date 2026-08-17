#ifndef TACHYON_SYSTEM_H
#define TACHYON_SYSTEM_H

#include "common/portability.h"
#include "common/type.h"
#include "common/vec.h"
#include <assert.h>
#include <setjmp.h>

#include "aphelion.h"

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

#define DEBUG_DEFINE

#if defined(DEBUG_DEFINE)
    extern int global_debug_indent;
    #define DEBUG(msg, ...) do {\
        printf("%*s" msg, global_debug_indent, "" __VA_OPT__(,) __VA_ARGS__);\
        fflush(stdout);\
    } while (0)
    #define DEBUG_NOI(msg, ...) do {\
        printf(msg __VA_OPT__(,) __VA_ARGS__);\
        fflush(stdout);\
    } while (0)
    #define DEBUG_INDENT global_debug_indent += 4
    #define DEBUG_DEDENT global_debug_indent -= 4
#else
    #define DEBUG(...)
    #define DEBUG_NOI(...)
    #define DEBUG_INDENT
    #define DEBUG_DEDENT
#endif

typedef struct System System;
typedef struct Lp Lp;
typedef struct SystemBus SystemBus;
typedef struct Compiler Compiler;
typedef struct BusDevice BusDevice;

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
    u32 bits;
} EncodedInst;

#define TLB_SIZE 256

#define MAX_LPS 512

#define RAM_SLOT_MAX_SIZE_BITS 38 
#define RAM_SLOT_MAX_SIZE (1ull << RAM_SLOT_MAX_SIZE_BITS)
#define MAX_RAM_SLOTS 512
#define APHEL_PAGE_SIZE (16*KiB)

typedef struct RamSlot {
    u8* raw_memory;
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

typedef enum : u8 {
    PTE_DCACHE_WRITEBACK = 0,
    PTE_DCACHE_WRITECOMBINE = 2,
    PTE_DCACHE_NONE = 3,
} PteCachingMode;

typedef struct PteEntry {
    u64 v : 1;
    u64 w : 1;
    u64 x : 1;
    u64 k : 1;
    u64 g : 1;
    u64 h : 1;
    PteCachingMode c : 2;
    u64 unused : 6;
    u64 next   : 50;
} PteEntry;
static_assert(sizeof(PteEntry) == sizeof(u64));

typedef enum : u8 {
    PTE_MAP_L1, // 16 KiB == normal page size
    PTE_MAP_L2, // 32 MiB
    PTE_MAP_L3, // 64 GiB
} PteMagnitude;

static inline u64 pte_magnitude_to_size(PteMagnitude magnitude) {
    switch (magnitude) {
    case PTE_MAP_L1: return 16*KiB;
    case PTE_MAP_L2: return 32*MiB;
    case PTE_MAP_L3: return 64*GiB;
    default: ASSUME(false);
    }
}

/// Translation cache entry
typedef struct TlbEntry {
    // ASID associated with this entry
    u64 asid;
    // ignore ASID comparison
    u64 global : 1;
    // only valid in kernel mode
    u64 only_kernel : 1;
    // can write
    u64 can_write : 1;
    // can execute
    u64 can_execute : 1;
    // size of mapping (for huge pages)
    PteMagnitude magnitude : 3;
    // physical page index
    u64 phys_page_index : 50;
} TlbEntry;


/// Translation cache
typedef struct LpTlb {
    TlbEntry entries[TLB_SIZE];
} LpTlb;

typedef struct LpLockState {
    u64 addr;
    u8 width;
    bool locked;
} LpLockState;

typedef struct LpInterruptState {
    jmp_buf interrupt_handler;
} LpInterruptState;


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

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

    UOP_PARAM_LP,
    UOP_PARAM_TABLE,

    UOP_GET_SREG,
    UOP_PUT_SREG,

    UOP_SET_I32,

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

    UOP_ADD_I32,
    UOP_SUB_I32,

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
    EXIT_NEXT_BLOCK = 1,

    // exit the test.
    EXIT_QUIT,

    EXIT_INTERRUPT_START,
    EXIT_INTERRUPT_END = EXIT_INTERRUPT_START + ICAUSE__COUNT,

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

#define MUST_TAIL [[gnu::musttail]]
#define SYSV_ABI [[gnu::sysv_abi]]
#define NO_CALLER_SAVED [[gnu::no_caller_saved_registers]]
#define JIT_HELPER SYSV_ABI NO_CALLER_SAVED

typedef struct JitHelperTable JitHelperTable;
struct JitHelperTable {
    void (*quit_with_code SYSV_ABI)      (Lp* lp, const JitHelperTable* table, BlockExitCode code);

    void (*translate_vaddr JIT_HELPER)   (Lp* lp, const JitHelperTable* table, u64 vaddr, AccessKind kind);
    void (*verify_raw_access JIT_HELPER) (Lp* lp, const JitHelperTable* table, u64 paddr, AccessWidth width, AccessKind kind);
    void (*raw_write JIT_HELPER)         (Lp* lp, const JitHelperTable* table, u64 paddr, AccessWidth width, u64 data);
    u64  (*raw_read JIT_HELPER)          (Lp* lp, const JitHelperTable* table, u64 paddr, AccessWidth width);
};

typedef noreturn void(*JitEntrypoint SYSV_ABI)(Lp* lp, const JitHelperTable* table);

typedef struct CompiledBlock CompiledBlock;
struct CompiledBlock {
    JitEntrypoint code;
    u32 code_size;
};

/// Create a block compiler.
Compiler compiler_new();
/// Compile a block, add it to the block map, and return it.
CompiledBlock* compiler_compile_block(Compiler* c, EncodedInst* binary, u64 paddr);
/// Retrieve a block from the block map. Returns `nullptr` if not found.
CompiledBlock* compiler_retrieve_block(Compiler* c, u64 paddr);
// Free a specific compiled block from the block map.
void compiler_free_block(Compiler* c, CompiledBlock* block);
/// Free compiled blocks with paddrs in [start, end)
void compiler_free_in_range(Compiler* c, u64 paddr_start, u64 paddr_end);
/// Free all blocks associated with this compiler.
void compiler_free_all(Compiler* c);

#define BLOCK_HASHMAP_CAPACITY_P2 12
#define BLOCK_HASHMAP_CAPACITY (1u << BLOCK_HASHMAP_CAPACITY_P2)

typedef struct BlockManager {
    CompiledBlock (*blocks)[BLOCK_HASHMAP_CAPACITY];

    /// A contiguous region for compiled code storage.
    u8* exec_region;
    /// Size of executable region, in bytes. must be a multiple of the target's page size.
    u32 exec_region_size;
    /// Number of executable bytes used (including possibly-freed).
    u32 exec_region_used;
    /// Number of executable bytes freed since last cleanup.
    u32 exec_region_freed;
    // Number of blocks allocated;
    u32 num_blocks_allocd;
} BlockManager;

typedef struct Compiler {
    BlockManager manager;

    /// Bookkeeping stuff

    Vec(UOp) uops;
    Vec(u8) code;

    EncodedInst* binary;
    u32 cursor;

    struct {
        /// Most recent definition of this sreg, 
        /// or UOP_INDEX_NULL if not defined in this block.
        UOpIndex recent_def;

        bool non_zero    : 1;
        bool write_safe  : 1;
        bool read_safe   : 1;
        bool modified    : 1;
    } sreg_status[GPR__COUNT];

    struct {
        /// The offset (from the start of the block) that IP currently holds.
        /// This starts at zero, but is updated any time that IP is stored to.
        /// Used to accurately update IP.
        usize offset_start;
    } ip;
} Compiler;

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

typedef struct Lp {
    u64 gpr[GPR__COUNT];
    u64 ctrl[CTRL__COUNT];

    System* sys;

    LpLockState llsc_lock;
    LpInterruptState interrupt_state;
    Compiler compiler;
    LpTlb tlb;
} Lp;


u64  lp_translate_addr(Lp* lp, u64 va, AccessKind kind);
void lp_check_physical_access(Lp* lp, u64 paddr, AccessWidth width, AccessKind kind);
u8*  lp_physical_get_haddr(Lp* lp, u64 paddr);
void lp_physical_write(Lp* lp, u64 paddr, AccessWidth width, void* data);
u64  lp_physical_read(Lp* lp, u64 paddr, AccessWidth width);

void lp_write(Lp* lp, u64 vaddr, AccessWidth width, void* data);
u64  lp_read(Lp* lp, u64 vaddr, AccessWidth width);

struct BusDevice {
    u64 range_start;
    u64 range_end;

    // A LP wants to read from a physical address.
    void (*respond_read_rq)(Lp* lp, u64 paddr, AccessWidth width, void* data);
    // A LP wants to fetch an instruction from a physical address.
    void (*respond_fetch_rq)(Lp* lp, u32* data);
    // A LP wants to write from a physical address.
    void (*respond_write_rq)(Lp* lp, u64 paddr, AccessWidth width, void* data);
};

typedef enum : u8 {
    SYS_MODE_STANDARD,
    SYS_MODE_SANDBOX,
} SystemMode;

struct System {
    Lp* lps;
    u16 lps_len;

    SystemBus bus;
    SystemMode mode;
};

System* system_init(u16 num_lps, u16 num_ram_slots, usize* ram_slot_sizes);
void system_dump(System* sys);
void system_launch(System* sys);

// Trigger an interrupt and longjmp back to the main execution loop.
noreturn void lp_trigger_interrupt(Lp* lp, u8 cause);
// Dump the lp's state to stdout.
void lp_dump_info(Lp* lp);

#endif // TACHYON_CORE_H
