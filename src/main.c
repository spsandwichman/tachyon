#include "asm/simple-asm.h"
#include "aphelion.h"
#include "common/str.h"
#include "common/util.h"
#include "common/vec.h"
#include "common/fs.h"
#include "system.h"
#include "tomlc17/tomlc17.h"
#include <stdlib.h>
#include <string.h>

// char test_config[] = {
//     #embed "../tests/arith/add.toml"
// };

static System* system_from_config(toml_datum_t config) {
    
    u16 num_lps = 1;
    Vec(usize) ram_slots = vec_new(usize, 8);

    auto init_tab = toml_get(config, "init");
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
        if (ram_item.type != TOML_TABLE) {
            printf("config: 'init.ram' must be an array of tables\n");
            exit(1);
        }

        // size = "[N] [Unit]"
        auto ram_slot_size = toml_get(ram_item, "size");
        if (ram_slot_size.type == TOML_STRING) {
            char* units;
            usize amount = strtoll(ram_slot_size.u.s, &units, 10);

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

            if (amount % APHEL_PAGE_SIZE != 0) {
                printf("config: RAM slot size '%zu' is not divisble by page size %zu\n", amount, (usize)APHEL_PAGE_SIZE);
                exit(1);
            }

            amount /= APHEL_PAGE_SIZE;

            vec_append(&ram_slots, amount);
        } 
        else {
            printf("config: table in 'init.ram' must have a 'size' field\n");
            exit(1);
        }
    }

    System* sys = system_new(num_lps, vec_len(ram_slots), ram_slots);

    for_n(i, 0, ram_array.u.arr.size) {
        auto ram_item = ram_array.u.arr.elem[i];

        auto asm_text = toml_get(ram_item, "asm");
        auto image_path = toml_get(ram_item, "image");
        if (image_path.type == TOML_STRING) {
            assert(asm_text.type == TOML_UNKNOWN);
            FsFile* binary_file = fs_open(image_path.u.s, false, false);
            string blob = fs_read_entire(binary_file, false);

            memcpy(sys->bus.ram_slots[i].raw_memory, blob.raw, blob.len);

            fs_close(binary_file);
            fs_destroy(binary_file);
        } 
        else if (asm_text.type == TOML_STRING) {
            assert(image_path.type == TOML_UNKNOWN);
            u32* ram_cursor = (u32*)sys->bus.ram_slots[i].raw_memory;
            const char* text = asm_text.u.s;
            while (true) {
                while (*text == ' ' || *text == '\t') {
                    text++;
                }
                if (*text == '\0') {
                    break;
                }

                char* eol = strchr(text, '\n');
                if (eol == text) {
                    break;
                }

                string line = {
                    .raw = (char*)text,
                    .len = eol - text,
                };
                *ram_cursor = encode_inst(line);
                ram_cursor++;
                text = eol + 1;
            }
        }
    }

    auto lps = toml_get(init_tab, "lps");
    if (lps.type == TOML_ARRAY) {
        for_n(i, 0, lps.u.arr.size) {
            auto lp_table = lps.u.arr.elem[i];

            auto gpr_table = toml_get(lp_table, "gpr");
            if (gpr_table.type == TOML_UNKNOWN) {
                continue;
            }
            if (gpr_table.type != TOML_TABLE) {
                printf("config: 'gpr' must be a table\n");
                exit(1);
            }
            for_n(gpr, 0, GPR__COUNT) {
                const char* name = gpr_name[gpr];
                auto gpr_value = toml_get(gpr_table, name);
                if (gpr_value.type != TOML_INT64) {
                    continue;
                }
                sys->lps[i].gpr[gpr] = gpr_value.u.int64;
            }
        }
    }

    auto mode = toml_get(init_tab, "mode");
    if (mode.type != TOML_STRING) {
        printf("config: mode invalid/unspecified, default to \"standard\"\n");
        sys->mode = SYS_MODE_STANDARD;
    } else if (!strcmp(mode.u.s, "standard")) {
        sys->mode = SYS_MODE_STANDARD;
    } else if (!strcmp(mode.u.s, "sandbox")) {
        sys->mode = SYS_MODE_SANDBOX;
    } else {
        DEBUG("config: mode invalid/unspecified, default to \"standard\"\n");
        sys->mode = SYS_MODE_STANDARD;
    }


    return sys;
}

static void check_expect(System* sys, toml_datum_t config) {
    auto expect_tab = toml_get(config, "expect");
    if (expect_tab.type == TOML_UNKNOWN) {
        return;
    }
    
    auto lp_array = toml_get(expect_tab, "lps");
    for_n(i, 0, lp_array.u.arr.size) {
        auto lp = lp_array.u.arr.elem[i];
        
        auto gpr_table = toml_get(lp, "gpr");
        if (gpr_table.type == TOML_UNKNOWN) {
            continue;
        }
        if (gpr_table.type != TOML_TABLE) {
            printf("config: 'gpr' must be a table\n");
            exit(1);
        }
        for_n(gpr, 0, GPR__COUNT) {
            const char* name = gpr_name[gpr];
            auto gpr_value = toml_get(gpr_table, name);
            if (gpr_value.type != TOML_INT64) {
                continue;
            }
            if (gpr_value.u.int64 != sys->lps[i].gpr[gpr]) {
                CRASH("expected GPR '%s' in LP %zu to be 0x%lx, got 0x%lx", 
                    name, i, 
                    gpr_value.u.int64,
                    sys->lps[i].gpr[gpr]
                );
            }
        }
    }
}


static void print_help() {
    puts("tachyon test <path/to/test(s)>");
}

static void run_test_toml(const char* config_path, string config_text) {
    toml_result_t config = toml_parse(config_text.raw, config_text.len);
    if (!config.ok) {
        printf("failed to parse test config '%s': %s\n", config_path, config.errmsg);
        exit(1);
    }

    System* sys = system_from_config(config.toptab);
    assert(sys->mode == SYS_MODE_SANDBOX);

    system_dump(sys);
    system_launch(sys);

    check_expect(sys, config.toptab);

    toml_free(config);
}

static string string_concat3(string a, string b, string c) {
    string d = string_alloc(a.len + b.len + c.len);
    memcpy(&d.raw[0], a.raw, a.len);
    memcpy(&d.raw[a.len], b.raw, b.len);
    memcpy(&d.raw[a.len + b.len], c.raw, c.len);
    return d;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        print_help();
        exit(0);
    }

    const char* subcommand = argv[1];
    
    if (strcmp(subcommand, "test") == 0) {

        Vec(string) tests = vec_new(string, 128);
        vec_append(&tests, string_clone(string_wrap(argv[2])));

        // iteratively expand
        for_n(i, 0, vec_len(tests)) {
            char* dir = clone_to_cstring(tests[i]);
            
            usize begin = vec_len(tests);
            fs_dir_contents(dir, &tests);
            usize end = vec_len(tests);

            for_n(j, begin, end) {
                string local_path = tests[j];
                tests[j] = string_concat3(tests[i], strlit("/"), local_path);
                // free(local_path.raw);
            }

            free(dir);
        }

        for_n(i, 0, vec_len(tests)) {
            if (!string_ends_with(tests[i], string_wrap(".toml"))) {
                continue;
            }
            char* test_path = clone_to_cstring(tests[i]);

            FsFile* config_file = fs_open(test_path, false, false);
            if (config_file == nullptr) {
                printf("cannot open '%s'\n", test_path);
                exit(1);
            }

            string config_text = fs_read_entire(config_file, true);

            run_test_toml(test_path, config_text);

            printf("passed '%s'\n", test_path);

            free(config_text.raw);
            fs_destroy(config_file);
            free(test_path);
        }
    }
}
