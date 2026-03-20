#include "common/util.h"
#include "common/vec.h"
#include "common/fs.h"
#include "system.h"
#include "compiler.h"
#include "tomlc17/tomlc17.h"


char test_config[] = {
    #embed "../tests/test1.toml"
};

int main() {

    DEBUG("test mode\n");

    u16 num_lps = 1;
    Vec(u32) ram_slots = vec_new(u32, 8);

    toml_result_t config = toml_parse(test_config, sizeof(test_config));
    if (!config.ok) {
        printf("failed to parse test config: %s\n", config.errmsg);
        exit(1);
    }
    DEBUG("config found...");
    {
        auto init_tab = toml_get(config.toptab, "system");
        if (init_tab.type == TOML_UNKNOWN) {
            printf("config: 'system' table not found\n");
            exit(1);
        }

        // get ram and image
    }
    DEBUG("loaded\n");

    // parse toml config

    DEBUG("system init...");
    System* sys = system_init(num_lps, vec_len(ram_slots), ram_slots);
    DEBUG("ok\n");

    DEBUG("lp count: %u\n", sys->lps_len);
    DEBUG("ram: \n");
    for_n(i, 0, sys->bus.ram_slots_len) {
        u64 start_addr = i * RAM_SLOT_MAX_SIZE;
        u64 slot_size = sys->bus.ram_slots[i].size_in_pages * PAGE_SIZE;
        DEBUG("     0x%016zx to 0x%016zx (%lu bytes)\n", 
            start_addr,
            start_addr + slot_size - 1,
            slot_size
        );
    }

    toml_free(config);

    Lp* lp0 = &sys->lps[0];
    
    lp_dump_info(lp0);

    compile_block(lp0, (EncodedInst*)&test_binary);
}
