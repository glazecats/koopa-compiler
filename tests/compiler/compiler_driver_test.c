#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_compiler_parses_supported_modes(void) {
    CompilerMode mode;

    if (!compiler_mode_from_flag("-riscv", &mode) || mode != COMPILER_MODE_RISCV) {
        fprintf(stderr, "[compiler] FAIL: -riscv mode parse mismatch\n");
        return 0;
    }
    if (!compiler_mode_from_flag("-perf", &mode) || mode != COMPILER_MODE_PERF) {
        fprintf(stderr, "[compiler] FAIL: -perf mode parse mismatch\n");
        return 0;
    }
    if (compiler_mode_from_flag("-koopa", &mode)) {
        fprintf(stderr, "[compiler] FAIL: unsupported mode unexpectedly accepted\n");
        return 0;
    }
    return 1;
}

static int test_compiler_builds_riscv_backend_dump_from_source(void) {
    static const char *source = "int main(){return 0;}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".attribute arch, \"rv32im\"\n.text\n.p2align 2\n\n.globl main\n.type main, @function\nmain:\n.Lmain_0:\n") != output ||
        strstr(output, "  li a0, 0\n") == NULL ||
        strstr(output, "  ret\n") == NULL ||
        strstr(output, ".size main, .-main\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: riscv compile output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_perf_backend_dump_from_source(void) {
    static const char *source = "int main(){return 1;}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_PERF, &output, &error) ||
        !output ||
        strstr(output, ".attribute arch, \"rv32im\"\n.text\n.p2align 2\n\n.globl main\n.type main, @function\nmain:\n.Lmain_0:\n") != output ||
        strstr(output, "  li a0, 1\n") == NULL ||
        strstr(output, "  ret\n") == NULL ||
        strstr(output, ".size main, .-main\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: perf compile output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_basic_riscv_mnemonics(void) {
    static const char *source = "int main(){int a; a=2; return a+3;}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  li a0, 5\n") == NULL ||
        strstr(output, "  ret\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: pretty riscv mnemonic output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_calls_and_control_labels(void) {
    static const char *source = "int foo(){return 3;} int main(){return foo();}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl foo\n.type foo, @function\nfoo:\n.Lfoo_0:\n") == NULL ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n.Lmain_0:\n") == NULL ||
        strstr(output, "  jal ra, foo\n") == NULL ||
        strstr(output, "  ret\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: call/control mnemonic output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_loads_and_branches(void) {
    static const char *source = "int choose(int x){ if (x) return 2; return 3; } int main(){ return choose(1); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  lw a0, 0(sp)\n") == NULL ||
        strstr(output, "  bne a0, zero, .Lchoose_2\n") == NULL ||
        strstr(output, ".Lchoose_1:\n") == NULL ||
        strstr(output, ".Lchoose_2:\n") == NULL ||
        strstr(output, "  jal ra, choose\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: load/branch mnemonic output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_rv32m_and_compare_branches(void) {
    static const char *source =
        "int divm(int a,int b){ return a/b + a%b; }"
        "int gt(int a,int b){ if (a>b) return a; return b; }"
        "int main(){ return divm(9,4) + gt(1,2); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  div a2, a1, a0\n") == NULL ||
        strstr(output, "  rem a0, a1, a0\n") == NULL ||
        strstr(output, "  blt a1, a2, .Lgt_2\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: rv32m/compare-branch mnemonic output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_external_call_and_stack_spill(void) {
    static const char *source =
        "int ext(int a,int b,int c,int d,int e,int f,int g,int h,int i);"
        "int main(){ return ext(1,2,3,4,5,6,7,8,9); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  addi sp, sp, -16\n") == NULL ||
        strstr(output, "  sw t5, 0(sp)\n") == NULL ||
        strstr(output, "  jal ra, ext\n") == NULL ||
        strstr(output, "  addi sp, sp, 16\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: external-call/spill mnemonic output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_immediate_compares_and_loop_control(void) {
    static const char *source =
        "int testeq(int a){ if (a==3) return 1; return 0; }"
        "int loop(int n){ int i; i=0; while(i<n){ i=i+1; } return i; }"
        "int main(){ return testeq(loop(3)); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  li t5, 3\n") == NULL ||
        strstr(output, "  beq a0, t5, .Ltesteq_2\n") == NULL ||
        strstr(output, ".globl loop\n.type loop, @function\nloop:\n.Lloop_0:\n") == NULL ||
        strstr(output, "  blt a1, a0, .Lloop_3\n") == NULL ||
        strstr(output, "  j .Lloop_1\n") == NULL ||
        strstr(output, "  jal ra, loop\n") == NULL ||
        strstr(output, "  jal ra, testeq\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: immediate-compare/loop-control mnemonic output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_emits_global_bss_and_data_sections(void) {
    static const char *bss_source = "int g; int main(){ return g; }\n";
    static const char *data_source = "int g = 7; int main(){ return g; }\n";
    static const char *multi_source = "int g; int h; int main(){ return h; }\n";
    static const char *store_source = "int g; int set(){ g = 5; return 0; } int main(){ set(); return g; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(bss_source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output,
            ".attribute arch, \"rv32im\"\n.section .sbss,\"aw\",@nobits\n.globl g\n.type g, @object\n.size g, 4\n.p2align 2\ng:\n  .zero 4\n\n.text\n") !=
            output ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  lw a0, %lo(g)(t5)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: global bss output mismatch: %s\n", error.message);
        ok = 0;
    }
    free(output);
    output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(data_source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output,
            ".attribute arch, \"rv32im\"\n.section .sdata,\"aw\",@progbits\n.globl g\n.type g, @object\n.size g, 4\n.p2align 2\ng:\n  .word 7\n\n.text\n") !=
            output ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  lw a0, %lo(g)(t5)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: global data output mismatch: %s\n", error.message);
        ok = 0;
    }
    free(output);
    output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(multi_source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl g\n.type g, @object\n.size g, 4\n.p2align 2\ng:\n  .zero 4\n") == NULL ||
        strstr(output, ".globl h\n.type h, @object\n.size h, 4\n.p2align 2\nh:\n  .zero 4\n") == NULL ||
        strstr(output, "  lui t5, %hi(h)\n") == NULL ||
        strstr(output, "  lw a0, %lo(h)(t5)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: multi-global gp-offset output mismatch: %s\n", error.message);
        ok = 0;
    }
    free(output);
    output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(store_source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  li t6, 5\n") == NULL ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  sw t6, %lo(g)(t5)\n") == NULL ||
        strstr(output, "  jal ra, set\n") == NULL ||
        strstr(output, "  lw a0, %lo(g)(t5)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: global store output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_compiler_parses_supported_modes();
    ok &= test_compiler_builds_riscv_backend_dump_from_source();
    ok &= test_compiler_builds_perf_backend_dump_from_source();
    ok &= test_compiler_pretty_prints_basic_riscv_mnemonics();
    ok &= test_compiler_pretty_prints_calls_and_control_labels();
    ok &= test_compiler_pretty_prints_loads_and_branches();
    ok &= test_compiler_pretty_prints_rv32m_and_compare_branches();
    ok &= test_compiler_pretty_prints_external_call_and_stack_spill();
    ok &= test_compiler_pretty_prints_immediate_compares_and_loop_control();
    ok &= test_compiler_emits_global_bss_and_data_sections();

    if (!ok) {
        return 1;
    }

    printf("[compiler] PASS\n");
    return 0;
}
