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
        strstr(output, ".weak _start\n.type _start, @function\n_start:\n") == NULL ||
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
        strstr(output, ".weak _start\n.type _start, @function\n_start:\n") == NULL ||
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
        strstr(output, "  blt a1, a0, .Lloop_3\n") == NULL ||
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
        strstr(output, "  li a0, 46\n") == NULL ||
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

    if (strstr(output,
            "  li a7, 8\n"
            "  call sum\n"
            "  sw a0, 4(sp)\n") == NULL ||
        strstr(output,
            "  li a7, 8\n"
            "  call sum2\n"
            "  addi sp, sp, 32\n") == NULL ||
        strstr(output,
            "  addi sp, sp, 32\n"
            "  lw t6, 4(sp)\n"
            "  add a0, t6, a0\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: caller-save span ordering mismatch: %s\n", error.message);
        ok = 0;
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
    ok &= test_compiler_handles_complex_const_shadowing_scopes();
    ok &= test_compiler_saves_caller_regs_around_whole_call_span();
    ok &= test_compiler_preserves_a0_across_nested_call_spans();
    ok &= test_compiler_folds_sysy_int_literal_boundaries();
    ok &= test_compiler_handles_memory_ssa_loop_local_entry_seed_case();
    ok &= test_compiler_handles_memory_ssa_join_binary_limit_case();
    ok &= test_compiler_handles_strict_local_state_loop_returns();
    ok &= test_compiler_handles_extreme_arity_compile_smoke();
    ok &= test_compiler_pretty_prints_far_call_pseudo_for_giant_arity();

    if (!ok) {
        return 1;
    }

    printf("[compiler] PASS\n");
    return 0;
}
