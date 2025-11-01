/*
 * r5vm Advanced Test Runner
 * Supports register validation via .expect files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include "r5vm.h"

// ANSI colors
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GRAY    "\033[90m"

#define TEST_MEM_SIZE (64 * 1024)
#define MAX_REG_CHECKS 32

typedef struct {
    uint32_t reg_num;
    uint32_t expected;
    bool check;
} reg_check_t;

typedef struct {
    const char* name;
    const char* bin_path;
    uint32_t expected_a0;
    reg_check_t reg_checks[MAX_REG_CHECKS];
    uint32_t max_steps;
} test_spec_t;

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

void r5vm_error(r5vm_t* vm, const char* msg, uint32_t pc, uint32_t instr)
{
    (void)vm; // Unused parameter
    fprintf(stderr, "%sVM ERROR at PC=0x%08X: %s (instr=0x%08X)%s\n",
            COLOR_RED, pc, msg, instr, COLOR_RESET);
}

static bool load_binary(const char* path, uint8_t* mem, size_t mem_size)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    if (fsize <= 0 || (size_t)fsize > mem_size) {
        fclose(f);
        return false;
    }

    size_t nread = fread(mem, 1, (size_t)fsize, f);
    fclose(f);

    if (nread != (size_t)fsize) return false;
    if ((size_t)fsize < mem_size)
        memset(mem + fsize, 0, mem_size - fsize);

    return true;
}

// Parse register name to number (x0-x31, zero, ra, sp, etc.)
static int parse_reg_name(const char* name)
{
    static const char* abi_names[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };

    // Check ABI names
    for (int i = 0; i < 32; i++) {
        if (strcmp(name, abi_names[i]) == 0)
            return i;
    }

    // Check x0-x31 format
    if (name[0] == 'x' && isdigit(name[1])) {
        int reg = atoi(name + 1);
        if (reg >= 0 && reg <= 31)
            return reg;
    }

    return -1;
}

// Load .expect file (optional register expectations)
static bool load_expectations(const char* bin_path, test_spec_t* spec)
{
    // Create .expect filename from .bin filename
    char expect_path[512];
    snprintf(expect_path, sizeof(expect_path), "%s", bin_path);
    char* dot = strrchr(expect_path, '.');
    if (dot) {
        strcpy(dot, ".expect");
    } else {
        strcat(expect_path, ".expect");
    }

    FILE* f = fopen(expect_path, "r");
    if (!f) {
        return false; // No expectations file is OK
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        char* p = line;
        while (isspace(*p)) p++;
        if (*p == '#' || *p == '\0') continue;

        // Parse: <reg_name> = <value>
        char reg_name[32];
        uint32_t value;
        if (sscanf(line, "%31s = 0x%x", reg_name, &value) == 2 ||
            sscanf(line, "%31s = %u", reg_name, &value) == 2) {

            int reg_num = parse_reg_name(reg_name);
            if (reg_num >= 0 && reg_num < 32) {
                spec->reg_checks[reg_num].reg_num = reg_num;
                spec->reg_checks[reg_num].expected = value;
                spec->reg_checks[reg_num].check = true;
            }
        }
    }
    fclose(f);
    return true;
}

static const char* get_reg_name(int num)
{
    static const char* names[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };
    return (num >= 0 && num < 32) ? names[num] : "???";
}

static void dump_registers(const r5vm_t* vm, const test_spec_t* spec)
{
    fprintf(stderr, "\n%s=== Register Dump ===%s\n", COLOR_CYAN, COLOR_RESET);
    fprintf(stderr, "PC: 0x%08X\n\n", vm->pc);

    for (int i = 0; i < 32; i++) {
        bool mismatch = spec && spec->reg_checks[i].check &&
                       vm->regs[i] != spec->reg_checks[i].expected;

        const char* color = mismatch ? COLOR_RED : COLOR_RESET;

        fprintf(stderr, "%sx%-2d (%-4s): 0x%08X",
                color, i, get_reg_name(i), vm->regs[i]);

        if (spec && spec->reg_checks[i].check) {
            fprintf(stderr, "  [expect: 0x%08X]", spec->reg_checks[i].expected);
        }

        fprintf(stderr, "%s%s", COLOR_RESET, (i % 2 == 1) ? "\n" : "  ");
    }

    fprintf(stderr, "%s=====================%s\n\n", COLOR_CYAN, COLOR_RESET);
}

static bool run_test(test_spec_t* spec)
{
    tests_run++;

    printf("%s[TEST]%s %-40s ... ", COLOR_CYAN, COLOR_RESET, spec->name);
    fflush(stdout);

    // Load expectations if available
    load_expectations(spec->bin_path, spec);

    uint8_t* mem = calloc(TEST_MEM_SIZE, 1);
    if (!mem) {
        printf("%sFAIL%s (memory allocation)\n", COLOR_RED, COLOR_RESET);
        tests_failed++;
        return false;
    }

    if (!load_binary(spec->bin_path, mem, TEST_MEM_SIZE)) {
        printf("%sFAIL%s (cannot load binary)\n", COLOR_RED, COLOR_RESET);
        free(mem);
        tests_failed++;
        return false;
    }

    r5vm_t vm;
    if (!r5vm_init(&vm, mem, TEST_MEM_SIZE)) {
        printf("%sFAIL%s (VM init)\n", COLOR_RED, COLOR_RESET);
        free(mem);
        tests_failed++;
        return false;
    }

    r5vm_reset(&vm);

    uint32_t max_steps = spec->max_steps ? spec->max_steps : 10000;
    unsigned steps = r5vm_run(&vm, max_steps);

    bool passed = true;

    if (steps >= max_steps) {
        printf("%sFAIL%s (timeout after %u steps)\n", COLOR_RED, COLOR_RESET, steps);
        dump_registers(&vm, spec);
        passed = false;
    }
    else if (vm.a0 != spec->expected_a0) {
        printf("%sFAIL%s (a0=0x%08X, expected=0x%08X)\n",
               COLOR_RED, COLOR_RESET, vm.a0, spec->expected_a0);
        dump_registers(&vm, spec);
        passed = false;
    }
    else {
        // Check additional register expectations
        for (int i = 0; i < 32; i++) {
            if (spec->reg_checks[i].check) {
                if (vm.regs[i] != spec->reg_checks[i].expected) {
                    printf("%sFAIL%s (x%d=%s=0x%08X, expected=0x%08X)\n",
                           COLOR_RED, COLOR_RESET, i, get_reg_name(i),
                           vm.regs[i], spec->reg_checks[i].expected);
                    dump_registers(&vm, spec);
                    passed = false;
                    break;
                }
            }
        }
    }

    if (passed) {
        // Count expectations
        int num_checks = 0;
        for (int i = 0; i < 32; i++)
            if (spec->reg_checks[i].check) num_checks++;

        printf("%sPASS%s (%u steps", COLOR_GREEN, COLOR_RESET, steps);
        if (num_checks > 0)
            printf(", %d reg checks", num_checks);
        printf(")\n");
        tests_passed++;
    } else {
        tests_failed++;
    }

    r5vm_destroy(&vm);
    free(mem);
    return passed;
}

static void print_summary(void)
{
    printf("\n%s================================%s\n", COLOR_CYAN, COLOR_RESET);
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %s%d%s\n", COLOR_GREEN, tests_passed, COLOR_RESET);
    printf("Tests failed: %s%d%s\n",
           tests_failed > 0 ? COLOR_RED : COLOR_GREEN,
           tests_failed, COLOR_RESET);
    printf("%s================================%s\n", COLOR_CYAN, COLOR_RESET);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test.bin> [<test2.bin> ...]\n", argv[0]);
        return 1;
    }

    printf("%s=== r5vm Test Runner ===%s\n\n", COLOR_CYAN, COLOR_RESET);

    for (int i = 1; i < argc; i++) {
        test_spec_t spec = {0};
        spec.name = argv[i];
        spec.bin_path = argv[i];
        spec.expected_a0 = 0;
        spec.max_steps = 10000;

        run_test(&spec);
    }

    print_summary();
    return (tests_failed == 0) ? 0 : 1;
}

