#include "common/util.h"
#include "common/vec.h"
#include "common/fs.h"
#include "system.h"
#include "compiler.h"
#include "tomlc17/tomlc17.h"
#include <stdlib.h>
#include <string.h>


char test_config[] = {
    #embed "../tests/test1.toml"
};

int main() {

    DEBUG("test mode\n");

    u16 num_lps = 1;
    Vec(usize) ram_slots = vec_new(usize, 8);

    toml_result_t config = toml_parse(test_config, sizeof(test_config));
    if (!config.ok) {
        printf("failed to parse test config: %s\n", config.errmsg);
        exit(1);
    }
    DEBUG("config found...");

    auto init_tab = toml_get(config.toptab, "init");
    if (init_tab.type == TOML_UNKNOWN) {
        printf("config: 'init' table not found\n");
        exit(1);
    }

    auto ram_array = toml_get(init_tab, "ram");
    if (ram_array.type != TOML_ARRAY) {
        printf("config: 'init.ram' array not found\n");
        exit(1);
    }
    for_n(i, 0, ram_array.u.arr.size) {
        auto ram_item = ram_array.u.arr.elem[i];
        if (ram_item.type != TOML_STRING) {
            printf("config: 'init.ram' must be an array of strings\n");
            exit(1);
        }

        char* units;
        usize amount = strtoll(ram_item.u.s, &units, 10);

        while (units[0] == ' ') {
            units += 1;
        }
        
        if (!strcmp(units, "B")) {
            // nothing
        } else if (!strcmp(units, "KiB")) {
            amount *= KiB;
        } else if (!strcmp(units, "MiB")) {
            amount *= MiB;
        } else if (!strcmp(units, "GiB")) {
            amount *= GiB;
        } else {
            printf("config: unknown memory unit '%s'\n", units);
            exit(1);
        }

        if (amount % PAGE_SIZE != 0) {
            printf("config: RAM slot size '%zu' is not divisble by page size %zu\n", amount, (usize)PAGE_SIZE);
            exit(1);
        }

        amount /= PAGE_SIZE;

        vec_append(&ram_slots, amount);
    }

    // find ram image
    auto image_path = toml_get(init_tab, "image");
    if (image_path.type != TOML_STRING) {
        printf("config: string 'init.image' not found\n");
        exit(1);
    }
    
    FsFile* binary_file = fs_open(image_path.u.s, false, false);
    string blob = fs_read_entire(binary_file, false);

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

    DEBUG("loading image...");
    memcpy(sys->bus.ram_slots[0].raw_memory, blob.raw, blob.len);
    DEBUG("ok (%zu bytes)\n", blob.len);

    toml_free(config);

    Lp* lp0 = &sys->lps[0];
    
    lp_dump_info(lp0);

    // compile_block(lp0, (EncodedInst*)&test_binary);
}
