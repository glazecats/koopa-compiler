#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static int compiler_test_appendf(char **cursor, size_t *remaining, const char *fmt, ...) {
    va_list args;
    int wrote;

    if (!cursor || !*cursor || !remaining || !fmt || *remaining == 0) {
        return 0;
    }

    va_start(args, fmt);
    wrote = vsnprintf(*cursor, *remaining, fmt, args);
    va_end(args);
    if (wrote < 0 || (size_t)wrote >= *remaining) {
        return 0;
    }

    *cursor += (size_t)wrote;
    *remaining -= (size_t)wrote;
    return 1;
}

static int compiler_test_contains_fragments_in_order(const char *text,
                                                     const char *const *fragments,
                                                     size_t fragment_count) {
    const char *cursor;
    size_t index;

    if (!text || !fragments) {
        return 0;
    }
    cursor = text;
    for (index = 0; index < fragment_count; ++index) {
        const char *found;

        if (!fragments[index]) {
            return 0;
        }
        found = strstr(cursor, fragments[index]);
        if (!found) {
            return 0;
        }
        cursor = found + strlen(fragments[index]);
    }
    return 1;
}

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

static int test_compiler_skips_all_paths_return_check_by_default(void) {
    static const char *source = "int f(int x){ if (x) return 1; } int main(){ return f(0); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));

    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  li a0, 0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: default skip-all-paths-return-check output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_riscv_backend_dump_from_source(void) {
    static const char *source = "int main(){return 0;}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".attribute arch, \"rv32im\"\n.text\n.p2align 2\n") != output ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL ||
        strstr(output, ".Lmain_0:\n") == NULL ||
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
        strstr(output, ".attribute arch, \"rv32im\"\n.text\n.p2align 2\n") != output ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL ||
        strstr(output, ".Lmain_0:\n") == NULL ||
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
        (strstr(output, "  li a0, 5\n") == NULL &&
         strstr(output, "  addi a0, a0, 3\n") == NULL) ||
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
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL ||
        strstr(output, ".Lmain_0:\n") == NULL ||
        strstr(output, "  call foo\n") == NULL ||
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
        strstr(output, "  bne a0, zero, .Lchoose_2\n") == NULL ||
        strstr(output, ".Lchoose_1:\n") == NULL ||
        strstr(output, ".Lchoose_2:\n") == NULL ||
        strstr(output, "  call choose\n") == NULL ||
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
        strstr(output, "  blt ") == NULL ||
        strstr(output, ".Lgt_2:\n") == NULL ||
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
        strstr(output, "  call ext\n") == NULL ||
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
        strstr(output, ".globl loop\n.type loop, @function\nloop:\n") == NULL ||
        strstr(output, ".Lloop_0:\n") == NULL ||
        strstr(output, ".Lloop_1:\n") == NULL ||
        strstr(output, "  blt ") == NULL ||
        strstr(output, ".Lloop_3:\n") == NULL ||
        strstr(output, "  j .Lloop_1\n") == NULL ||
        strstr(output, "  call loop\n") == NULL ||
        strstr(output, "  call testeq\n") == NULL ||
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
        strstr(output, "  li a0, 7\n") == NULL) {
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
        strstr(output, "  call set\n") == NULL ||
        strstr(output, "  lw a0, %lo(g)(t5)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: global store output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_zero_global_store_with_symbol_fixups(void) {
    static const char *source =
        "int g; int h; int read(){ return g + h; }"
        "int main(){ g = 0; h = 0; return read(); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  sw zero, %lo(g)(t5)\n") == NULL ||
        strstr(output, "  lui t5, %hi(h)\n") == NULL ||
        strstr(output, "  sw zero, %lo(h)(t5)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: zero global store symbol fixup mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_pretty_prints_global_array_address_materialization(void) {
    static const char *source =
        "int g[2];\n"
        "int foo(int a[]){ return 0; }\n"
        "int main(){ foo(g); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl g\n.type g, @object\n.size g, 8\n.p2align 2\ng:\n  .zero 8\n") == NULL ||
        strstr(output, "  lui a0, %hi(g)\n") == NULL ||
        strstr(output, "  addi a0, a0, %lo(g)\n") == NULL ||
        strstr(output, "  call foo\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: global-array address materialization mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_elides_zero_offset_local_address_addition_in_text(void) {
    static const char *source =
        "int main(){ int a[4]; int b; a[0]=7; b=a[0]; return b; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  li a0, 0\n  add a0, a1, a0\n  lw a0, 0(a0)\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: zero-offset local address addition was not elided: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_reuses_stack_address_rematerialization_in_text(void) {
    static const char *source =
        "int main(){ int a[4]; int b; a[0]=7; b=a[0]; return b; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  addi a1, sp, 16\n  sw a1, 0(sp)\n  lw a0, 0(sp)\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-address rematerialization was not folded: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_folds_indexed_local_base_offset_in_text(void) {
    static const char *source =
        "int test(int a[][4], int n) { return a[0][n]; }\n"
        "int main() {\n"
        "  int n;\n"
        "  int a[1][1][4] = {0, 1, 2, 3};\n"
        "  n = getint();\n"
        "  int b[2][2] = {1, 2, 1, 2};\n"
        "  return test(a[0], n) + b[1][0];\n"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  lw a0, 0(sp)\n  lw a0, 4(sp)\n  slli a0, a0, 2\n  add a0, sp, a0\n  lw a0, 0(a0)\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: indexed local base offset fold misfired on array parameter access: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_folds_call_arg_load_swap_in_text(void) {
    static const char *source =
        "int f(int x, int y){ return x + y; }\n"
        "int main(){ int a[4]; int i; int y; a[0]=1; a[1]=2; a[2]=3; a[3]=4; i=2; y=5; return f(a[i], y); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  lw a1, 0(a0)\n  lw a0, 20(sp)\n  mv t5, a0\n  mv a0, a1\n  mv a1, t5\n  call f\n") != NULL ||
        (strstr(output, "  lw a0, 0(a0)\n  lw a1, 20(sp)\n  call f\n") == NULL &&
            strstr(output, "  lw a1, 0(a0)\n  mv a0, a1\n  li a1, 5\n  call f\n") == NULL &&
            strstr(output, "  lw a0, 0(a0)\n  li a1, 5\n  call f\n") == NULL &&
            strstr(output, "  lw a0, 0(a0)\n  addi a0, a0, 5\n") == NULL &&
            strstr(output, "  lw a0, 0(a1)\n  addi a0, a0, 5\n") == NULL)) {
        fprintf(stderr, "[compiler] FAIL: call-arg load swap was not folded: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

int compiler_test_optimize_riscv_preview_call_arg_load_swaps(char **io_text);
int compiler_test_optimize_riscv_preview_adjacent_stack_store_reload_to_mv(char **io_text);
int compiler_test_optimize_riscv_preview_tail_calls(char **io_text);
int compiler_test_optimize_riscv_preview_zero_adds(char **io_text);
int compiler_test_optimize_riscv_preview_mul_by_four(char **io_text);
int compiler_test_optimize_riscv_preview_sub_by_one(char **io_text);
int compiler_test_optimize_riscv_preview_stack_addr_reuse(char **io_text);
int compiler_test_optimize_riscv_preview_repeated_indexed_addr_triples(char **io_text);
int compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(char **io_text);
int compiler_test_optimize_riscv_preview_stack_staged_call_args(char **io_text);
int compiler_test_optimize_riscv_preview_same_block_temp_stack_reload_to_mv(char **io_text);
int compiler_test_optimize_riscv_preview_branch_bound_stack_reload_to_mv(char **io_text);
int compiler_test_optimize_riscv_preview_forward_store_copy_source(char **io_text);
int compiler_test_optimize_riscv_preview_remove_dead_jump_seed_moves(char **io_text);
int compiler_test_optimize_riscv_preview_fold_materialized_stack_slot_accesses(char **io_text);
int compiler_test_optimize_riscv_preview_indexed_local_base_offsets(char **io_text);
int compiler_test_optimize_riscv_preview_reuse_repeated_lui_addi_constants(char **io_text);
int compiler_test_optimize_riscv_preview_mod998_divmods(char **io_text);
int compiler_test_optimize_riscv_preview_pow2_divmods(char **io_text);
int compiler_test_optimize_riscv_preview_store_jump_increment_tails(char **io_text);

static int test_compiler_does_not_fold_call_arg_load_swap_when_second_base_is_a1(void) {
    static const char *source_text =
        "  lw a1, 0(a0)\n"
        "  lw a0, 0(a1)\n"
        "  mv t5, a0\n"
        "  mv a0, a1\n"
        "  mv a1, t5\n"
        "  call ext\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: call-arg swap regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_call_arg_load_swaps(&text) ||
        !text ||
        strstr(text, "  mv t5, a0\n  mv a0, a1\n  mv a1, t5\n  call ext\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: call-arg swap regression should keep the unsafe pattern unchanged\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_call_arg_load_swap_when_first_base_is_a0(void) {
    static const char *source_text =
        "  lw a1, 0(a0)\n"
        "  lw a0, 8(sp)\n"
        "  mv t5, a0\n"
        "  mv a0, a1\n"
        "  mv a1, t5\n"
        "  call ext\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: call-arg swap first-base regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_call_arg_load_swaps(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: call-arg swap regression should keep the a0-based first load unchanged\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_stack_staged_call_args_when_both_args_reload_same_slot(void) {
    static const char *source_text =
        "  lw t0, 16(sp)\n"
        "  sw t0, 0(sp)\n"
        "  lw t1, 20(sp)\n"
        "  sw t1, 0(sp)\n"
        "  lw a0, 0(sp)\n"
        "  lw a1, 0(sp)\n"
        "  call ext\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-staged-call-args same-slot regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_staged_call_args(&text) ||
        !text ||
        strstr(text, "  lw a0, 16(sp)\n  sw a0, 0(sp)\n  lw a1, 20(sp)\n  sw a1, 0(sp)\n  call ext\n") != NULL ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-staged call-arg fold should keep same-slot reload pattern unchanged\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_replaces_same_block_temp_stack_reload_with_mv(void) {
    static const char *source_text =
        "  sw t4, 36(sp)\n"
        "  lw t6, 36(sp)\n"
        "  add a0, t6, a1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: same-block temp stack-reload regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_same_block_temp_stack_reload_to_mv(&text) ||
        !text ||
        strstr(text, "  sw t4, 36(sp)\n  mv t6, t4\n  add a0, t6, a1\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: same-block temp stack reload should be replaced with mv when safe\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_replaces_adjacent_stack_store_reload_with_mv(void) {
    static const char *source_text =
        "  sw t4, 36(sp)\n"
        "  lw t5, 36(sp)\n"
        "  add a0, t5, a1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: adjacent stack store-reload regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_adjacent_stack_store_reload_to_mv(&text) ||
        !text ||
        strstr(text, "  sw t4, 36(sp)\n  mv t5, t4\n  add a0, t5, a1\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: adjacent stack store/reload should become mv when safe\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_removes_adjacent_stack_self_reload(void) {
    static const char *source_text =
        "  sw t4, 36(sp)\n"
        "  lw t4, 36(sp)\n"
        "  add a0, t4, a1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: adjacent stack self-reload regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_adjacent_stack_store_reload_to_mv(&text) ||
        !text ||
        strstr(text, "  sw t4, 36(sp)\n  add a0, t4, a1\n") == NULL ||
        strstr(text, "  lw t4, 36(sp)\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: adjacent stack self-reload should disappear when safe\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_replaces_branch_bound_stack_reload_with_mv(void) {
    static const char *source_text =
        "  sw t4, 36(sp)\n"
        "  lw t5, 36(sp)\n"
        "  blt a0, t5, .Lloop\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: branch-bound stack-reload regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_branch_bound_stack_reload_to_mv(&text) ||
        !text ||
        strstr(text, "  sw t4, 36(sp)\n  mv t5, t4\n  blt a0, t5, .Lloop\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: branch-bound stack reload should be replaced with mv when safe\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_replace_branch_bound_stack_reload_across_slot_overwrite(void) {
    static const char *source_text =
        "  sw t4, 36(sp)\n"
        "  sw t3, 36(sp)\n"
        "  lw t5, 36(sp)\n"
        "  blt a0, t5, .Lloop\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: branch-bound stack-reload overwrite regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_branch_bound_stack_reload_to_mv(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: branch-bound stack reload should stay unchanged across slot overwrite\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_forwards_store_copy_source(void) {
    static const char *source_text =
        "  mv t4, a3\n"
        "  lui t6, 0x1\n"
        "  addi t6, t6, -2016\n"
        "  add t6, sp, t6\n"
        "  sw t4, 0(t6)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: forward-store-copy-source regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_forward_store_copy_source(&text) ||
        !text ||
        strstr(text,
            "  lui t6, 0x1\n"
            "  addi t6, t6, -2016\n"
            "  add t6, sp, t6\n"
            "  sw a3, 0(t6)\n") == NULL ||
        strstr(text, "  mv t4, a3\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: store-copy source should be forwarded into the final sw when safe\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_forward_store_copy_source_across_copy_use(void) {
    static const char *source_text =
        "  mv t4, a3\n"
        "  add a0, t4, a1\n"
        "  sw t4, 0(t6)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: forward-store-copy-source use-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_forward_store_copy_source(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: store-copy source should stay unchanged when the copied reg is used before the store\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_removes_dead_jump_seed_move(void) {
    static const char *source_text =
        "  mv a0, a3\n"
        "  sw a3, 0(t6)\n"
        "  j .Ltail\n"
        ".Ltail:\n"
        "  lw t6, 0(t4)\n"
        "  addi a0, t6, 1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: dead-jump-seed-move regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_remove_dead_jump_seed_moves(&text) ||
        !text ||
        strstr(text, "  mv a0, a3\n") != NULL ||
        strstr(text,
            "  sw a3, 0(t6)\n"
            "  j .Ltail\n"
            ".Ltail:\n"
            "  lw t6, 0(t4)\n"
            "  addi a0, t6, 1\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: dead jump seed move should be removed when target redefines the register before use\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_remove_jump_seed_move_when_target_uses_it_first(void) {
    static const char *source_text =
        "  mv a0, a3\n"
        "  j .Ltail\n"
        ".Ltail:\n"
        "  add a1, a0, a2\n"
        "  addi a0, zero, 1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: dead-jump-seed-move target-use regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_remove_dead_jump_seed_moves(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: jump seed move should stay when the target block uses the register before redefining it\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_folds_materialized_stack_slot_load(void) {
    static const char *source_text =
        "  addi t6, sp, 24\n"
        "  lw a0, 0(t6)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: fold-materialized-stack-load regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_fold_materialized_stack_slot_accesses(&text) ||
        !text ||
        strstr(text, "  lw a0, 24(sp)\n") == NULL ||
        strstr(text, "  addi t6, sp, 24\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: materialized stack-slot load should fold into direct sp-offset load\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_folds_materialized_stack_slot_store(void) {
    static const char *source_text =
        "  addi t6, sp, 2016\n"
        "  sw a3, 0(t6)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: fold-materialized-stack-store regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_fold_materialized_stack_slot_accesses(&text) ||
        !text ||
        strstr(text, "  sw a3, 2016(sp)\n") == NULL ||
        strstr(text, "  addi t6, sp, 2016\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: materialized stack-slot store should fold into direct sp-offset store\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_folds_large_materialized_stack_slot_load(void) {
    static const char *source_text =
        "  lui t6, 0x1\n"
        "  addi t6, t6, -2036\n"
        "  add t6, sp, t6\n"
        "  lw a0, 0(t6)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: fold-large-materialized-stack-load regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_fold_materialized_stack_slot_accesses(&text) ||
        !text ||
        strstr(text, "  addi t6, sp, 2047\n  lw a0, 13(t6)\n") == NULL ||
        strstr(text, "  lui t6, 0x1\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: large materialized stack-slot load should fold into split sp-offset load\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_folds_large_materialized_stack_slot_store(void) {
    static const char *source_text =
        "  lui t6, 0x1\n"
        "  addi t6, t6, -2016\n"
        "  add t6, sp, t6\n"
        "  sw a3, 0(t6)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: fold-large-materialized-stack-store regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_fold_materialized_stack_slot_accesses(&text) ||
        !text ||
        strstr(text, "  addi t6, sp, 2047\n  sw a3, 33(t6)\n") == NULL ||
        strstr(text, "  lui t6, 0x1\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: large materialized stack-slot store should fold into split sp-offset store\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_stack_staged_call_args_when_stage_reg_is_a0(void) {
    static const char *source_text =
        "  lw a0, 16(sp)\n"
        "  sw a0, 0(sp)\n"
        "  lw t1, 20(sp)\n"
        "  sw t1, 4(sp)\n"
        "  lw a0, 0(sp)\n"
        "  lw a1, 4(sp)\n"
        "  call ext\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-staged-call-args a0-stage regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_staged_call_args(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-staged call-arg fold should keep the a0-staged pattern unchanged\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_tail_call_when_restore_instructions_separate_jal_and_epilogue(void) {
    static const char *source_text =
        "  jal ra, foo\n"
        "  lw t0, 0(s11)\n"
        "  lw s11, 4(sp)\n"
        "  lw ra, 8(sp)\n"
        "  addi sp, sp, 12\n"
        "  ret\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: tail-call regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_tail_calls(&text) ||
        !text ||
        strstr(text, "  jal ra, foo\n  lw t0, 0(s11)\n  lw s11, 4(sp)\n  lw ra, 8(sp)\n  addi sp, sp, 12\n  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: tail-call fold should not cross intervening s11 restore instructions\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_elide_zero_add_when_zero_reg_crosses_label(void) {
    static const char *source_text =
        "  li a1, 0\n"
        "  add a0, a2, a1\n"
        ".Lkeep_zero:\n"
        "  add a3, a1, a4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: zero-add cross-label regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_zero_adds(&text) ||
        !text ||
        strstr(text, "  li a1, 0\n  add a0, a2, a1\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: zero-add fold should keep zero materialization when the reg is needed past a label\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_stack_address_when_same_slot_is_overwritten_via_materialized_base(void) {
    static const char *source_text =
        "  addi t0, sp, 24\n"
        "  sw t0, 0(sp)\n"
        "  addi t6, sp, 24\n"
        "  sw a1, 0(t6)\n"
        "  lw a0, 0(sp)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-address materialized-store regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_addr_reuse(&text) ||
        !text ||
        strstr(text, "  addi a0, sp, 24\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-address reuse should not fire when the saved slot is overwritten through a materialized stack base\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_stack_address_when_tmp_reg_is_needed_past_label(void) {
    static const char *source_text =
        "  addi t0, sp, 24\n"
        "  sw t0, 0(sp)\n"
        "  lw a0, 0(sp)\n"
        ".Lkeep_tmp:\n"
        "  add a1, t0, a2\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-address cross-label regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_addr_reuse(&text) ||
        !text ||
        strstr(text, "  addi t0, sp, 24\n  sw t0, 0(sp)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-address reuse should keep tmp materialization when the tmp reg is needed past a label\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_stack_address_across_call_when_saved_addr_reg_survives_call(void) {
    static const char *source_text =
        "  addi s1, sp, 24\n"
        "  sw s1, 0(sp)\n"
        "  call foo\n"
        "  lw s2, 0(sp)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-address call-barrier callee-saved regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_addr_reuse(&text) ||
        !text ||
        strstr(text, "  addi s1, sp, 24\n  sw s1, 0(sp)\n  call foo\n  lw s2, 0(sp)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-address reuse should not cross a call barrier just because the saved address register is callee-saved\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_stack_address_before_call_when_later_reload_still_needs_saved_slot(void) {
    static const char *source_text =
        "  addi t0, sp, 24\n"
        "  sw t0, 0(sp)\n"
        "  lw a1, 0(sp)\n"
        "  call foo\n"
        "  lw a0, 0(sp)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-address later-reload regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_addr_reuse(&text) ||
        !text ||
        strstr(text,
            "  addi t0, sp, 24\n"
            "  sw t0, 0(sp)\n"
            "  lw a1, 0(sp)\n"
            "  call foo\n"
            "  lw a0, 0(sp)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-address reuse should not delete the saved stack-slot definition when a later reload after the call still needs it\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_stack_address_across_ret_barrier(void) {
    static const char *source_text =
        "  addi t0, sp, 24\n"
        "  sw t0, 0(sp)\n"
        "  ret\n"
        "  lw a0, 0(sp)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: stack-address ret-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_stack_addr_reuse(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: stack-address reuse should stop at ret barrier\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_mul_by_four_when_scale_reg_is_needed_past_label(void) {
    static const char *source_text =
        "  li a1, 4\n"
        "  mul a0, a2, a1\n"
        ".Lkeep_scale:\n"
        "  mul a3, a4, a1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mul-by-four cross-label regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mul_by_four(&text) ||
        !text ||
        strstr(text, "  li a1, 4\n  mul a0, a2, a1\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mul-by-four fold should keep scale materialization when the reg is needed past a label\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_sub_by_one_when_one_reg_is_needed_past_label(void) {
    static const char *source_text =
        "  li a1, 1\n"
        "  sub a0, a2, a1\n"
        ".Lkeep_one:\n"
        "  sub a3, a4, a1\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: sub-by-one cross-label regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_sub_by_one(&text) ||
        !text ||
        strstr(text, "  li a1, 1\n  sub a0, a2, a1\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: sub-by-one fold should keep one materialization when the reg is needed past a label\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_rewrites_pow2_div_and_mod(void) {
    static const char *source_text =
        "  li t5, 2\n"
        "  div a0, t6, t5\n"
        "  li t4, 2\n"
        "  rem a1, t6, t4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: pow2-divmod regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_pow2_divmods(&text) ||
        !text ||
        strstr(text, "  div a0, t6, t5\n") != NULL ||
        strstr(text, "  rem a1, t6, t4\n") != NULL ||
        strstr(text, "  srai t4, t6, 31\n") == NULL ||
        strstr(text, "  andi t4, t4, 1\n") == NULL ||
        strstr(text, "  add a0, t6, t4\n") == NULL ||
        strstr(text, "  srai a0, a0, 1\n") == NULL ||
        strstr(text, "  add t5, t6, t4\n") == NULL ||
        strstr(text, "  srai t5, t5, 1\n") == NULL ||
        strstr(text, "  slli t5, t5, 1\n") == NULL ||
        strstr(text, "  sub a1, t6, t5\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: pow2 div/mod rewrite did not trigger as expected\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_rewrites_mod998_div_only(void) {
    static const char *source_text =
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  div a0, t6, t5\n"
        "  lui t4, 0x3b800\n"
        "  addi t4, t4, 1\n"
        "  rem a1, t6, t4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mod998-divmod regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mod998_divmods(&text) ||
        !text ||
        strstr(text, "  div a0, t6, t5\n") != NULL ||
        strstr(text, "  rem a1, t6, t4\n") == NULL ||
        strstr(text, "  lui t3, 70493\n") == NULL ||
        strstr(text, "  addi t3, t3, -2031\n") == NULL ||
        strstr(text, "  mulh a0, t6, t3\n") == NULL ||
        strstr(text, "  srli t3, a0, 31\n") == NULL ||
        strstr(text, "  srai a0, a0, 26\n") == NULL ||
        strstr(text, "  add a0, a0, t3\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mod998 div rewrite did not trigger as expected\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_rewrite_mod998_div_when_t3_needed_past_label(void) {
    static const char *source_text =
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  div a0, t6, t5\n"
        ".Lkeep_t3:\n"
        "  add a1, t3, t4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mod998-div live-t3 regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mod998_divmods(&text) ||
        !text ||
        strstr(text, "  div a0, t6, t5\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mod998 div rewrite should stay disabled when t3 is needed past a label\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_rewrites_mod998_rem_only(void) {
    static const char *source_text =
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a0, t6, t5\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mod998_divmods(&text) ||
        !text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem regression setup failed during optimize\n");
        ok = 0;
        goto cleanup;
    }

    if (strstr(text, "  rem a0, t6, t5\n") != NULL ||
        strstr(text, "  mulh t4, t6, t3\n") == NULL ||
        strstr(text, "  sub a0, t6, t4\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mod998 rem rewrite did not trigger as expected\n");
        ok = 0;
    }

cleanup:
    free(text);
    return ok;
}

static int test_compiler_does_not_rewrite_mod998_rem_when_t3_needed_past_label(void) {
    static const char *source_text =
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a0, t6, t5\n"
        ".Lkeep_t3:\n"
        "  add a1, t3, t4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem live-t3 regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mod998_divmods(&text) ||
        !text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem live-t3 regression setup failed during optimize\n");
        ok = 0;
        goto cleanup;
    }

    if (strstr(text, "  rem a0, t6, t5\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mod998 rem rewrite should stay disabled when t3 is needed past a label\n");
        ok = 0;
    }

cleanup:
    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_stack_slot_store(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n"
        "  sw a1, 24(sp)\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-triple regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_triples(&text) ||
        !text ||
        strstr(text, "  sw a1, 24(sp)\n  slli a0, a2, 2\n  lw t6, 24(sp)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed triple fold should keep recomputation when the base slot is overwritten\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_base_store(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n"
        "  sw a1, 0(s1)\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence store regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, "  sw a1, 0(s1)\n  slli a0, a2, 2\n  lw t6, 0(s1)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should keep recomputation when the loaded base memory is overwritten\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_base_reg_redefinition(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n"
        "  mv s1, a3\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence base-reg regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, "  mv s1, a3\n  slli a0, a2, 2\n  lw t6, 0(s1)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should keep recomputation when the load base register is redefined\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_loaded_base_reg_redefinition(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n"
        "  addi t6, t6, 4\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence loaded-base-reg regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, "  addi t6, t6, 4\n  slli a0, a2, 2\n  lw t6, 0(s1)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should keep recomputation when the loaded base register is redefined\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_materialized_stack_slot_store(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n"
        "  addi t5, sp, 24\n"
        "  sw a1, 0(t5)\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-triple materialized-store regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_triples(&text) ||
        !text ||
        strstr(text, "  sw a1, 0(t5)\n  slli a0, a2, 2\n  lw t6, 24(sp)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed triple fold should keep recomputation when the stack base slot is overwritten via materialized stack address\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_materialized_stack_slot_store(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n"
        "  addi t5, sp, 24\n"
        "  sw a1, 0(t5)\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence materialized-store regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, "  sw a1, 0(t5)\n  slli a0, a2, 2\n  lw t6, 24(sp)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should keep recomputation when the stack-loaded base slot is overwritten via materialized stack address\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_call(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n"
        "  call foo\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-triple call-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_triples(&text) ||
        !text ||
        strstr(text, "  call foo\n  slli a0, a2, 2\n  lw t6, 24(sp)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed triple fold should keep recomputation across call barrier\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_call_when_regs_survive_call(void) {
    static const char *source_text =
        "  slli s3, s4, 2\n"
        "  lw s2, 24(sp)\n"
        "  add s3, s2, s3\n"
        "  call foo\n"
        "  slli s3, s4, 2\n"
        "  lw s2, 24(sp)\n"
        "  add s3, s2, s3\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-triple callee-saved call-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_triples(&text) ||
        !text ||
        strstr(text, "  call foo\n  slli s3, s4, 2\n  lw s2, 24(sp)\n  add s3, s2, s3\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed triple fold should not cross a call barrier just because its helper regs are callee-saved\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_call(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n"
        "  call foo\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence call-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, "  call foo\n  slli a0, a2, 2\n  lw t6, 0(s1)\n  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should keep recomputation across call barrier\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_call_when_regs_survive_call(void) {
    static const char *source_text =
        "  slli s3, s4, 2\n"
        "  lw s2, 0(s1)\n"
        "  add s3, s2, s3\n"
        "  call foo\n"
        "  slli s3, s4, 2\n"
        "  lw s2, 0(s1)\n"
        "  add s3, s2, s3\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence callee-saved call-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, "  call foo\n  slli s3, s4, 2\n  lw s2, 0(s1)\n  add s3, s2, s3\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should not cross a call barrier just because its helper regs are callee-saved\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_jump_barrier(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n"
        "  j .Ldone\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence jump-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should stop at jump barrier\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_non_sp_base(void) {
    static const char *source_text =
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n"
        "  slli a0, a2, 2\n"
        "  lw t6, 0(s1)\n"
        "  add a0, t6, a0\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: repeated-indexed-sequence non-sp-base regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed sequence fold should not rewrite non-sp bases\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_indexed_local_base_offset_across_ret_barrier(void) {
    static const char *source_text =
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n"
        "  ret\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: indexed-local-base-offset ret-barrier regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_indexed_local_base_offsets(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: indexed local base offset fold should stop at ret barrier\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_indexed_local_base_offset_across_stack_slot_overwrite(void) {
    static const char *source_text =
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n"
        "  sw a3, 24(sp)\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: indexed-local-base-offset stack overwrite regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_indexed_local_base_offsets(&text) ||
        !text ||
        strstr(text, "  sw a3, 24(sp)\n  add a0, t6, a2\n  lw a1, 0(a0)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: indexed local base offset fold should keep recomputation when the stack-held base pointer slot is overwritten\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_indexed_local_base_offset_across_materialized_stack_slot_overwrite(void) {
    static const char *source_text =
        "  lw t6, 24(sp)\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n"
        "  addi t5, sp, 24\n"
        "  sw a3, 0(t5)\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: indexed-local-base-offset materialized overwrite regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_indexed_local_base_offsets(&text) ||
        !text ||
        strstr(text, "  sw a3, 0(t5)\n  add a0, t6, a2\n  lw a1, 0(a0)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: indexed local base offset fold should keep recomputation when the stack-held base pointer slot is overwritten through a materialized stack address\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_indexed_local_base_offset_when_base_reg_is_needed_past_label(void) {
    static const char *source_text =
        "  addi t6, sp, 24\n"
        "  add a0, t6, a2\n"
        "  lw a1, 0(a0)\n"
        ".Lkeep_base:\n"
        "  add a3, t6, a4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: indexed-local-base-offset cross-label regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_indexed_local_base_offsets(&text) ||
        !text ||
        strstr(text, "  addi t6, sp, 24\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: indexed local base offset fold should keep base-reg materialization when the base reg is needed past a label\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_indexed_local_base_offset_when_index_reg_is_same_as_base_reg(void) {
    static const char *source_text =
        "  addi t6, sp, 24\n"
        "  add a0, t6, t6\n"
        "  lw a1, 0(a0)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: indexed-local-base-offset same-reg regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_indexed_local_base_offsets(&text) ||
        !text ||
        strstr(text, "  addi t6, sp, 24\n  add a0, t6, t6\n  lw a1, 0(a0)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: indexed local base offset fold should not fire when the index reg is the same as the base reg\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_removes_repeated_indexed_address_sequence_in_text(void) {
    static const char *source =
        "const int base = 16;\n"
        "int getNumPos(int num, int pos){\n"
        "  int i=0;\n"
        "  while(i<pos){ num = num / base; i = i + 1; }\n"
        "  return num % base;\n"
        "}\n"
        "int f(int bitround, int a[], int t){\n"
        "  int head[base] = {};\n"
        "  head[0] = 0;\n"
        "  head[1] = 1;\n"
        "  int v = a[head[getNumPos(t, bitround)]];\n"
        "  a[head[getNumPos(t, bitround)]] = t;\n"
        "  head[getNumPos(t, bitround)] = head[getNumPos(t, bitround)] + 1;\n"
        "  return v + head[getNumPos(t, bitround)];\n"
        "}\n"
        "int g[2];\n"
        "int main(){ g[0]=1; g[1]=0; return f(0, g, g[1]); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output,
            "  slli a0, a2, 2\n"
            "  lw t6, 256(sp)\n"
            "  add a0, t6, a0\n"
            "  lw t4, 0(a0)\n"
            "  sw t4, 228(sp)\n"
            "  slli a0, a2, 2\n"
            "  lw t6, 256(sp)\n"
            "  add a0, t6, a0\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed address sequence was not removed: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_reuses_repeated_indexed_address_sequence_in_text(void) {
    static const char *source =
        "int f(int a[], int idx, int v) {\n"
        "  int x = a[idx];\n"
        "  a[idx] = v;\n"
        "  return x;\n"
        "}\n"
        "int g[4];\n"
        "int main(){ g[0]=1; g[1]=2; return f(g, 1, 3); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  slli a0, a2, 2\n  add a0, t6, a0\n  lw t4, 0(a0)\n  sw t4, 8(sp)\n  slli a0, a2, 2\n  add a0, t6, a0\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated indexed address sequence was not reused: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_reuses_repeated_lui_addi_constant_in_text(void) {
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(512u);
    if (!text) {
        return 0;
    }
    strcpy(text,
        ".Lhot:\n"
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a0, a0, t5\n"
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  add a0, a0, t5\n");

    if (!compiler_test_optimize_riscv_preview_reuse_repeated_lui_addi_constants(&text) ||
        !text ||
        strstr(text,
            "  lui t5, 0x3b800\n"
            "  addi t5, t5, 1\n"
            "  rem a0, a0, t5\n"
            "  add a0, a0, t5\n") == NULL ||
        strstr(text,
            "  rem a0, a0, t5\n"
            "  lui t5, 0x3b800\n"
            "  addi t5, t5, 1\n"
            "  add a0, a0, t5\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated lui/addi constant materialization was not reused\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_reuse_repeated_lui_addi_constant_across_call(void) {
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(512u);
    if (!text) {
        return 0;
    }
    strcpy(text,
        ".Lhot:\n"
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a0, a0, t5\n"
        "  call multiply\n"
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  add a0, a0, t5\n");

    if (!compiler_test_optimize_riscv_preview_reuse_repeated_lui_addi_constants(&text) ||
        !text ||
        strstr(text,
            "  call multiply\n"
            "  lui t5, 0x3b800\n"
            "  addi t5, t5, 1\n"
            "  add a0, a0, t5\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated lui/addi constant should not be reused across call barrier\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_handles_complex_const_shadowing_scopes(void) {
    static const char *source =
        "int main() {\n"
        "  int a = 1, sum = 0;\n"
        "  {\n"
        "    a = a + 2;\n"
        "    int b = a + 3;\n"
        "    b = b + 4;\n"
        "    sum = sum + a + b;\n"
        "    {\n"
        "      b = b + 5;\n"
        "      int c = b + 6;\n"
        "      a = a + c;\n"
        "      sum = sum + a + b + c;\n"
        "      {\n"
        "        b = b + a;\n"
        "        int a = c + 7;\n"
        "        a = a + 8;\n"
        "        sum = sum + a + b + c;\n"
        "        {\n"
        "          b = b + a;\n"
        "          int b = c + 9;\n"
        "          a = a + 10;\n"
        "          const int a = 11;\n"
        "          b = b + 12;\n"
        "          sum = sum + a + b + c;\n"
        "          {\n"
        "            c = c + b;\n"
        "            int c = b + 13;\n"
        "            c = c + a;\n"
        "            sum = sum + a + b + c;\n"
        "          }\n"
        "          sum = sum - c;\n"
        "        }\n"
        "        sum = sum - b;\n"
        "      }\n"
        "      sum = sum - a;\n"
        "    }\n"
        "  }\n"
        "  return sum % 77;\n"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        (strstr(output, "  li a0, 46\n") == NULL &&
         strstr(output, "  rem a0, a0, t5\n") == NULL) ||
        strstr(output, "  ret\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: complex const shadowing output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_saves_caller_regs_around_whole_call_span(void) {
    static const char *source =
        "int sum(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7){"
        "return a0+a1+a2+a3+a4+a5+a6+a7;}"
        "int sum2(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,"
        "int a9,int a10,int a11,int a12,int a13,int a14,int a15){"
        "return a0+a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12+a13+a14+a15;}"
        "int main(){int x=sum(1,2,3,4,5,6,7,8); return x+sum2(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: caller-save span compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    {
        static const char *const first_call_flow[] = {
            "  li a7, 8\n",
            "  call sum\n",
            "  sw a0, 0(sp)\n",
        };
        static const char *const second_call_flow[] = {
            "  li a7, 8\n",
            "  call sum2\n",
            "  addi sp, sp, 32\n",
        };
        static const char *const merge_flow[] = {
            "  addi sp, sp, 32\n",
            "  mv a1, a0\n",
            "  lw a0, 0(sp)\n",
            "  add a0, a1, a0\n",
        };
        if (!compiler_test_contains_fragments_in_order(output, first_call_flow,
                sizeof(first_call_flow) / sizeof(first_call_flow[0])) ||
            !compiler_test_contains_fragments_in_order(output, second_call_flow,
                sizeof(second_call_flow) / sizeof(second_call_flow[0])) ||
            !compiler_test_contains_fragments_in_order(output, merge_flow,
                sizeof(merge_flow) / sizeof(merge_flow[0]))) {
        fprintf(stderr, "[compiler] FAIL: caller-save span ordering mismatch: %s\n", error.message);
        ok = 0;
        }
    }

    free(output);
    return ok;
}

static int test_compiler_preserves_a0_across_nested_call_spans(void) {
    static const char *source =
        "int f(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,"
        "int a8,int a9,int a10,int a11,int a12,int a13,int a14,int a15){"
        "return a0+a1+a2+a3+a4+a5+a6+a7+a8+a9+a10+a11+a12+a13+a14+a15;}"
        "int g0(){return 1;} int g1(){return 2;} int g2(){return 3;} int g3(){return 4;}"
        "int g4(){return 5;} int g5(){return 6;} int g6(){return 7;} int g7(){return 8;}"
        "int g8(){return 9;} int g9(){return 10;} int g10(){return 11;} int g11(){return 12;}"
        "int g12(){return 13;} int g13(){return 14;} int g14(){return 15;} int g15(){return 16;}"
        "int main(){return f(g0(),g1(),g2(),g3(),g4(),g5(),g6(),g7(),g8(),g9(),g10(),g11(),g12(),g13(),g14(),g15());}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: nested-call a0 preserve compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    if (strstr(output, "  sw a0, 92(sp)\n") == NULL ||
        strstr(output, "  sw a0, 32(sp)\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: nested-call a0 preserve output mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_restores_large_frame_s11_before_restoring_sp(void) {
    static const char *source =
        "int g(int x) { return x + 1; }\n"
        "int f(int n) {\n"
        "  int a[600];\n"
        "  a[0] = n;\n"
        "  a[599] = g(n);\n"
        "  return a[0] + a[599];\n"
        "}\n"
        "int main() { return f(3); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (setenv("COMPILER_USE_DIRECT_SIMPLE_TEXT", "1", 1) != 0) {
        fprintf(stderr, "[compiler] FAIL: large-frame s11 restore setup failed\n");
        return 0;
    }
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  li t6, -2416\n") == NULL ||
        strstr(output, "  add sp, sp, t6\n") == NULL ||
        strstr(output, "  li t6, 2416\n") == NULL ||
        strstr(output, "  add sp, sp, t6\n  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: large-frame s11 restore ordering mismatch: %s\n", error.message);
        ok = 0;
    }
    unsetenv("COMPILER_USE_DIRECT_SIMPLE_TEXT");
    free(output);
    return ok;
}

static int test_compiler_folds_sysy_int_literal_boundaries(void) {
    static const char *source =
        "int main() {"
        "  const int k0 = -2147483648;"
        "  const int k1 = 0x80000000;"
        "  const int k2 = 0x80000000 + 1;"
        "  const int k3 = 0x7fffffff;"
        "  const int k4 = 0x7fffffff - 1;"
        "  return ((k0 == k1) << 0)"
        "       + ((k0 + 1 == k2) << 1)"
        "       + ((k0 == -k3 - 1) << 2)"
        "       + ((k0 == k4 + 1) << 3)"
        "       + ((k1 / 2 == k2 / 2) << 4)"
        "       + ((k1 == -k3 - 1) << 5)"
        "       + ((k1 == k4 + 1) << 6);"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  li a0, 39\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: sysy int-literal boundary folding mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_handles_32bit_wraparound_loop_condition(void) {
    static const char *source =
        "int main(){"
        "  int x = 2147483647;"
        "  x = x + 1;"
        "  while(x < 0){"
        "    return 1;"
        "  }"
        "  return 0;"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  blt a0, zero, .Lmain_3\n") == NULL ||
        strstr(output, "  li a0, 1\n") == NULL ||
        strstr(output, "  li a0, 0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: 32-bit wraparound loop-condition compile mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_handles_memory_ssa_loop_local_entry_seed_case(void) {
    static const char *source =
        "int main(){"
        "int day,x1,x2;"
        "day=9;"
        "x2=1;"
        "while(day>0){"
        "x1=(x2+1)*2;"
        "x2=x1;"
        "day=day-1;"
        "}"
        "return x1;"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: memory-ssa loop-local entry-seed compile mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_handles_memory_ssa_join_binary_limit_case(void) {
    static const char *source =
        "int main(){"
        "int m,n,temp,i;"
        "m=getint();"
        "n=getint();"
        "if(m<n){temp=m; m=n; n=temp;}"
        "i=m;"
        "while(i>0){"
        "if(i%m==0 && i%n==0){putint(i); break;}"
        "i=i+1;"
        "}"
        "return 0;"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: memory-ssa join-binary compile mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_handles_strict_local_state_loop_returns(void) {
    static const char *while_source =
        "int f(){int b=1; while(b<2){ if(b){ return 2; } }}\n";
    static const char *for_source =
        "int f(){int b=1; for(;b<2;b=b+1){ if(b){ return 2; } }}\n";
    static const char *alias_source =
        "int f(int a){while(1){if(a){return 1;}int b=a;b=1;for(;b<2;b=b+1){if(b){return 2;}}}}\n";
    CompilerError error;
    CompilerOptions options;
    char *output = NULL;
    int ok = 1;

    memset(&options, 0, sizeof(options));
    options.skip_all_paths_return_check = 0;
    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text_with_options(while_source, COMPILER_MODE_RISCV, &options, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: strict local-state while compile mismatch: %s\n", error.message);
        ok = 0;
    }
    free(output);
    output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text_with_options(for_source, COMPILER_MODE_RISCV, &options, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: strict local-state for compile mismatch: %s\n", error.message);
        ok = 0;
    }
    free(output);
    output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text_with_options(alias_source, COMPILER_MODE_RISCV, &options, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: strict local-state alias compile mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_treats_sysy_32bit_wraparound_conditions_as_true_runtime_paths(void) {
    static const char *source =
        "int f(){int x=2147483647; x=x+1; while(x<0){ return 1; } return 0; }\n";
    CompilerOptions options;
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&options, 0, sizeof(options));
    options.skip_all_paths_return_check = 0;
    memset(&error, 0, sizeof(error));

    if (!compiler_compile_source_text_with_options(source, COMPILER_MODE_RISCV, &options, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  blt a0, zero, .Lf_3\n") == NULL ||
        strstr(output, "  li a0, 1\n") == NULL ||
        strstr(output, "  li a0, 0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: 32-bit wraparound condition runtime-path mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_preserves_assignment_condition_break_loop_exit(void) {
    static const char *source =
        "int foo(){return 1;}"
        "int main(){ int b=1; while(1){ if(b = foo()) break; } return 3; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  call foo\n") == NULL ||
        strstr(output, "  beq a0, zero, .Lmain_5\n") == NULL ||
        strstr(output, "  li a0, 3\n") == NULL ||
        strstr(output, "  j .Lmain_1\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: assignment-condition break loop exit compile mismatch: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_does_not_fold_mutated_global_condition_from_initializer(void) {
    static const char *source =
        "int g=0;"
        "int f(int x){ g = x; if (g < 0) return 1; return 0; }\n"
        "int main(){ return f(getint()); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl f\n.type f, @function\nf:\n") == NULL ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  sw a0, %lo(g)(t5)\n") == NULL ||
        strstr(output, "  blt a0, zero, ") == NULL ||
        strstr(output, "  li a0, 1\n") == NULL ||
        strstr(output, "  call getint\n") == NULL ||
        strstr(output, "  call f\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: mutated-global condition should not fold from initializer metadata: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_keeps_global_reload_after_same_block_global_writing_call(void) {
    static const char *source =
        "int s=2;"
        "int f0(){ s=s+1; return 0; }"
        "int f1(){ s=s+2; return 1; }"
        "int h(){ s=s+3; return s%5; }"
        "int pick(int x,int y){ return x*10+y; }"
        "int main(){ int c=f1(); int u = c ? (f1()) : (f0()); int v = pick(u, h()); return s + u*10 + v*100; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) || !output) {
        fprintf(stderr,
            "[compiler] FAIL: same-block global-writing call should preserve later global reload: %s\n",
            error.message);
        ok = 0;
    } else {
        static const char *const reload_flow[] = {
            "  call h\n",
            "  call pick\n",
            "  lui t5, %hi(s)\n",
            "  lw a0, %lo(s)(t5)\n",
        };
        if (strstr(output, ".globl h\n.type h, @function\nh:\n") == NULL ||
            !compiler_test_contains_fragments_in_order(output, reload_flow,
                sizeof(reload_flow) / sizeof(reload_flow[0]))) {
            fprintf(stderr,
                "[compiler] FAIL: same-block global-writing call should preserve later global reload: %s\n",
                error.message);
            ok = 0;
        }
    }

    free(output);
    return ok;
}

static int test_compiler_does_not_pretty_print_self_moves(void) {
    static const char *source =
        "const int mod = 998244353;\n"
        "int d;\n"
        "int power(int a, int b){ if (b == 0) return 1; return a; }\n"
        "int fft(int arr[], int begin_pos, int n, int w){ return 0; }\n"
        "int main(){ d = 1; fft(0, 0, d, power(3, (mod - 1) / d)); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  mv t6, t6\n") != NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: self-move pretty-print mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_handles_extreme_arity_compile_smoke(void) {
    enum { kArgCount = 300, kSourceCapacity = 65536 };
    CompilerError error;
    char *source = NULL;
    char *cursor = NULL;
    char *output = NULL;
    size_t remaining = 0;
    int ok = 1;
    int index;

    source = (char *)malloc(kSourceCapacity);
    if (!source) {
        fprintf(stderr, "[compiler] FAIL: extreme arity source allocation failed\n");
        return 0;
    }

    cursor = source;
    remaining = kSourceCapacity;
    if (!compiler_test_appendf(&cursor, &remaining, "int pick(")) {
        ok = 0;
        goto cleanup;
    }
    for (index = 0; index < kArgCount; ++index) {
        if (!compiler_test_appendf(&cursor, &remaining, "%sint a%d", index == 0 ? "" : ",", index)) {
            ok = 0;
            goto cleanup;
        }
    }
    if (!compiler_test_appendf(&cursor, &remaining, "){ return a%d; } int main(){ return pick(", kArgCount - 1)) {
        ok = 0;
        goto cleanup;
    }
    for (index = 0; index < kArgCount; ++index) {
        if (!compiler_test_appendf(&cursor, &remaining, "%s%d", index == 0 ? "" : ",", index)) {
            ok = 0;
            goto cleanup;
        }
    }
    if (!compiler_test_appendf(&cursor, &remaining, "); }\n")) {
        ok = 0;
        goto cleanup;
    }

    memset(&error, 0, sizeof(error));
    if (!ok ||
        !compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl pick\n.type pick, @function\npick:\n") == NULL ||
        strstr(output, "  call pick\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: extreme arity compile smoke mismatch: %s\n", error.message);
        ok = 0;
    }

cleanup:
    free(output);
    free(source);
    return ok;
}

static int test_compiler_pretty_prints_far_call_pseudo_for_giant_arity(void) {
    enum { kArgCount = 1030, kSourceCapacity = 262144 };
    CompilerError error;
    char *source = NULL;
    char *cursor = NULL;
    char *output = NULL;
    size_t remaining = 0;
    int ok = 1;
    int index;

    source = (char *)malloc(kSourceCapacity);
    if (!source) {
        fprintf(stderr, "[compiler] FAIL: far-call source allocation failed\n");
        return 0;
    }

    cursor = source;
    remaining = kSourceCapacity;
    if (!compiler_test_appendf(&cursor, &remaining, "int sink(")) {
        ok = 0;
        goto cleanup;
    }
    for (index = 0; index < kArgCount; ++index) {
        if (!compiler_test_appendf(&cursor, &remaining, "%sint a%d", index == 0 ? "" : ",", index)) {
            ok = 0;
            goto cleanup;
        }
    }
    if (!compiler_test_appendf(&cursor, &remaining, "){ return a0; } int main(){ return sink(")) {
        ok = 0;
        goto cleanup;
    }
    for (index = 0; index < kArgCount; ++index) {
        if (!compiler_test_appendf(&cursor, &remaining, "%s%d", index == 0 ? "" : ",", index)) {
            ok = 0;
            goto cleanup;
        }
    }
    if (!compiler_test_appendf(&cursor, &remaining, "); }\n")) {
        ok = 0;
        goto cleanup;
    }

    memset(&error, 0, sizeof(error));
    if (!ok ||
        !compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl sink\n.type sink, @function\nsink:\n") == NULL ||
        strstr(output, "  call sink\n") == NULL ||
        strstr(output, ".4byte") != NULL) {
        fprintf(stderr, "[compiler] FAIL: far-call pseudo compile mismatch: %s\n", error.message);
        ok = 0;
    }

cleanup:
    free(output);
    free(source);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_compiler_parses_supported_modes();
    ok &= test_compiler_skips_all_paths_return_check_by_default();
    ok &= test_compiler_builds_riscv_backend_dump_from_source();
    ok &= test_compiler_builds_perf_backend_dump_from_source();
    ok &= test_compiler_pretty_prints_basic_riscv_mnemonics();
    ok &= test_compiler_pretty_prints_calls_and_control_labels();
    ok &= test_compiler_pretty_prints_loads_and_branches();
    ok &= test_compiler_pretty_prints_rv32m_and_compare_branches();
    ok &= test_compiler_pretty_prints_external_call_and_stack_spill();
    ok &= test_compiler_pretty_prints_immediate_compares_and_loop_control();
    ok &= test_compiler_emits_global_bss_and_data_sections();
    ok &= test_compiler_pretty_prints_zero_global_store_with_symbol_fixups();
    ok &= test_compiler_pretty_prints_global_array_address_materialization();
    ok &= test_compiler_elides_zero_offset_local_address_addition_in_text();
    ok &= test_compiler_reuses_stack_address_rematerialization_in_text();
    ok &= test_compiler_folds_indexed_local_base_offset_in_text();
    ok &= test_compiler_folds_call_arg_load_swap_in_text();
    ok &= test_compiler_does_not_fold_call_arg_load_swap_when_second_base_is_a1();
    ok &= test_compiler_does_not_fold_call_arg_load_swap_when_first_base_is_a0();
    ok &= test_compiler_does_not_fold_stack_staged_call_args_when_both_args_reload_same_slot();
    ok &= test_compiler_does_not_fold_stack_staged_call_args_when_stage_reg_is_a0();
    ok &= test_compiler_replaces_same_block_temp_stack_reload_with_mv();
    ok &= test_compiler_replaces_adjacent_stack_store_reload_with_mv();
    ok &= test_compiler_removes_adjacent_stack_self_reload();
    ok &= test_compiler_replaces_branch_bound_stack_reload_with_mv();
    ok &= test_compiler_does_not_replace_branch_bound_stack_reload_across_slot_overwrite();
    ok &= test_compiler_forwards_store_copy_source();
    ok &= test_compiler_does_not_forward_store_copy_source_across_copy_use();
    ok &= test_compiler_removes_dead_jump_seed_move();
    ok &= test_compiler_does_not_remove_jump_seed_move_when_target_uses_it_first();
    ok &= test_compiler_folds_materialized_stack_slot_load();
    ok &= test_compiler_folds_materialized_stack_slot_store();
    ok &= test_compiler_folds_large_materialized_stack_slot_load();
    ok &= test_compiler_folds_large_materialized_stack_slot_store();
    ok &= test_compiler_does_not_fold_tail_call_when_restore_instructions_separate_jal_and_epilogue();
    ok &= test_compiler_does_not_elide_zero_add_when_zero_reg_crosses_label();
    ok &= test_compiler_does_not_fold_mul_by_four_when_scale_reg_is_needed_past_label();
    ok &= test_compiler_does_not_fold_sub_by_one_when_one_reg_is_needed_past_label();
    ok &= test_compiler_rewrites_pow2_div_and_mod();
    ok &= test_compiler_rewrites_mod998_div_only();
    ok &= test_compiler_does_not_rewrite_mod998_div_when_t3_needed_past_label();
    ok &= test_compiler_rewrites_mod998_rem_only();
    ok &= test_compiler_does_not_rewrite_mod998_rem_when_t3_needed_past_label();
    ok &= test_compiler_does_not_reuse_stack_address_when_same_slot_is_overwritten_via_materialized_base();
    ok &= test_compiler_does_not_reuse_stack_address_when_tmp_reg_is_needed_past_label();
    ok &= test_compiler_does_not_reuse_stack_address_across_call_when_saved_addr_reg_survives_call();
    ok &= test_compiler_does_not_reuse_stack_address_before_call_when_later_reload_still_needs_saved_slot();
    ok &= test_compiler_does_not_reuse_stack_address_across_ret_barrier();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_stack_slot_store();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_materialized_stack_slot_store();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_call();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_triple_across_call_when_regs_survive_call();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_base_store();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_base_reg_redefinition();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_loaded_base_reg_redefinition();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_materialized_stack_slot_store();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_non_sp_base();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_jump_barrier();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_call();
    ok &= test_compiler_does_not_reuse_repeated_indexed_addr_sequence_across_call_when_regs_survive_call();
    ok &= test_compiler_does_not_fold_indexed_local_base_offset_across_ret_barrier();
    ok &= test_compiler_does_not_fold_indexed_local_base_offset_across_stack_slot_overwrite();
    ok &= test_compiler_does_not_fold_indexed_local_base_offset_across_materialized_stack_slot_overwrite();
    ok &= test_compiler_does_not_fold_indexed_local_base_offset_when_base_reg_is_needed_past_label();
    ok &= test_compiler_does_not_fold_indexed_local_base_offset_when_index_reg_is_same_as_base_reg();
    ok &= test_compiler_removes_repeated_indexed_address_sequence_in_text();
    ok &= test_compiler_reuses_repeated_indexed_address_sequence_in_text();
    ok &= test_compiler_reuses_repeated_lui_addi_constant_in_text();
    ok &= test_compiler_does_not_reuse_repeated_lui_addi_constant_across_call();
    ok &= test_compiler_handles_complex_const_shadowing_scopes();
    ok &= test_compiler_saves_caller_regs_around_whole_call_span();
    ok &= test_compiler_preserves_a0_across_nested_call_spans();
    ok &= test_compiler_restores_large_frame_s11_before_restoring_sp();
    ok &= test_compiler_folds_sysy_int_literal_boundaries();
    ok &= test_compiler_handles_32bit_wraparound_loop_condition();
    ok &= test_compiler_handles_memory_ssa_loop_local_entry_seed_case();
    ok &= test_compiler_handles_memory_ssa_join_binary_limit_case();
    ok &= test_compiler_handles_strict_local_state_loop_returns();
    ok &= test_compiler_treats_sysy_32bit_wraparound_conditions_as_true_runtime_paths();
    ok &= test_compiler_preserves_assignment_condition_break_loop_exit();
    ok &= test_compiler_does_not_fold_mutated_global_condition_from_initializer();
    ok &= test_compiler_keeps_global_reload_after_same_block_global_writing_call();
    ok &= test_compiler_does_not_pretty_print_self_moves();
    ok &= test_compiler_handles_extreme_arity_compile_smoke();
    ok &= test_compiler_pretty_prints_far_call_pseudo_for_giant_arity();

    if (!ok) {
        return 1;
    }

    printf("[compiler] PASS\n");
    return 0;
}
