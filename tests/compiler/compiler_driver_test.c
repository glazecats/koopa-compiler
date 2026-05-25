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

static int compiler_test_contains_putint_imm_sequence(const char *text,
                                                      const long long *values,
                                                      size_t value_count) {
    const char *cursor;
    size_t i;
    char fragment[64];

    if (!text || !values) {
        return 0;
    }

    cursor = text;
    for (i = 0; i < value_count; ++i) {
        snprintf(fragment, sizeof(fragment), "  li a0, %lld\n  call putint\n", values[i]);
        cursor = strstr(cursor, fragment);
        if (!cursor) {
            return 0;
        }
        cursor += strlen(fragment);
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
    if (!compiler_mode_from_flag("-extension", &mode) || mode != COMPILER_MODE_EXTENSION) {
        fprintf(stderr, "[compiler] FAIL: -extension mode parse mismatch\n");
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

static int test_compiler_rejects_defer_outside_extension_mode(void) {
    static const char *source = "int main(){ defer putint(1); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        strstr(error.message, "SEMA-EXT-001") == NULL) {
        fprintf(stderr, "[compiler] FAIL: defer should be rejected outside extension mode: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_defer_under_extension_with_return_check_enabled(void) {
    static const char *source = "int main(){ defer putint(1); return 0; }\n";
    CompilerError error;
    CompilerOptions options;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    memset(&options, 0, sizeof(options));
    options.skip_all_paths_return_check = 0;
    if (!compiler_compile_source_text_with_options(source,
            COMPILER_MODE_EXTENSION,
            &options,
            &output,
            &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: extension defer with return-check should compile: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_unless_outside_extension_mode(void) {
    static const char *source = "int main(){ unless(0) return 1; return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error)) {
        fprintf(stderr, "[compiler] FAIL: unless should be rejected outside extension mode\n");
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_unless_under_extension(void) {
    static const char *source = "int main(){ unless(0) putint(1); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  li a0, 1\n") == NULL ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension unless should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_float_transport_under_extension(void) {
    static const char *source = "float g; float id(float x){ return x; } int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl id\n.type id, @function\nid:\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: float transport slice should compile under extension: %s\n", error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_literal_transport_under_extension(void) {
    static const char *source = "float id(float x){ return x; } int main(){ float x = 1.25; id(1.25); return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl id\n.type id, @function\nid:\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: float literal transport slice should compile under extension: %s\n", error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_global_initializer_transport_from_identifier_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = g;\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float global initializer from identifier should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_global_initializer_transport_from_call_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float h = id(g);\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float global initializer from call should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_return_transport_from_global_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float get(){ return g; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float return from global should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_return_transport_from_global_call_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float get(){ return id(g); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float return from global call should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_parameter_forward_transport_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float forward(float x){ return id(x); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float parameter forward transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_parameter_local_forward_transport_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float bounce(float x){ float y; y = x; return id(y); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float parameter local forward transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_global_call_chain_transport_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float getg(){ return wrap(g); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float global call-chain transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_local_call_chain_transport_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float bounce(float x){ float y; y = x; return wrap(y); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float local call-chain transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_if_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ if(g) return 1; return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float if-condition should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_while_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ while(g) return 1; return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float while-condition should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_for_condition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ for(;g;) return 1; return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float for-condition should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_array_local_declaration_under_extension(void) {
    static const char *source = "int main(){ float a[2]; return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-037") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float array local declaration should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_array_global_declaration_under_extension(void) {
    static const char *source = "float a[2];\nint main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-037") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float array global declaration should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_array_parameter_under_extension(void) {
    static const char *source = "float f(float a[]){ return 0; }\nint main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-037") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float array parameter should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_operator_expression_with_global_literal_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ return g + 1; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float operator expression with global literal should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_operator_expression_in_top_level_initializer_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int h = g + 1;\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float operator expression in top-level initializer should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_call_result_in_top_level_initializer_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "int h = id(g);\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float call result in top-level initializer should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_assignment_transport_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "int main(){ float y; y = id(g); return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float assignment transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_assignment_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ int x = 0; x = g; return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float assignment to int should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_return_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int bad(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value return to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_return_to_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "int bad(){ return -id(1.0) ? 1.0 : 2.0; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value return to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_logical_condition_composition_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ if(!g || (g && 1.25)) return g ? 1 : 0; return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float logical condition composition should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_same_type_float_ternary_value_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 1.25;\n"
        "float mainf(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: same-type float ternary value should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float x, float y){ return x == y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float equality compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float x, float y){ return x < y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float relational compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_negative_float_literal_transport_under_extension(void) {
    static const char *source =
        "float neg(){ return -1.25; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: negative float literal transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_unary_minus_float_identifier_transport_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float neg(){ return -g; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: unary minus float identifier transport should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_negative_float_relational_compare_against_zero_under_extension(void) {
    static const char *source =
        "int lt(){ return -1.25 < 0.0; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: negative float compare against zero should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_addition_under_extension(void) {
    static const char *source =
        "float add(float x, float y){ return x + y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float addition should lower through helper under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_subtraction_under_extension(void) {
    static const char *source =
        "float sub(float x, float y){ return x - y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fsub32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float subtraction should lower through helper under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_negative_float_addition_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float add(float y){ return -g + y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: negative float addition combo should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_negative_float_subtraction_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float sub(float y){ return y - -g; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fsub32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: negative float subtraction combo should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_multiplication_under_extension(void) {
    static const char *source =
        "float mul(float x, float y){ return x * y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float multiplication should lower through helper under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_division_under_extension(void) {
    static const char *source =
        "float divv(float x, float y){ return x / y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float division should lower through helper under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_negative_float_multiplication_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float mul(float y){ return -g * y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: negative float multiplication combo should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_negative_float_division_combo_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float divv(float y){ return y / -g; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: negative float division combo should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_chained_float_addition_under_extension(void) {
    static const char *source =
        "float add3(float x, float y, float z){ return (x + y) + z; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: chained float addition should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_nested_float_mul_div_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ return -a * (b / c); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested float mul/div should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_chained_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float x, float y, float z){ return ((x + y) + z) == z; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: chained float equality compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_chained_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float x, float y, float z){ return ((x + y) + z) < z; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: chained float relational compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_nested_muldiv_float_equality_compare_under_extension(void) {
    static const char *source =
        "int eq(float a, float b, float c){ return (-a * (b / c)) == c; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested mul/div float equality compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_nested_muldiv_float_relational_compare_under_extension(void) {
    static const char *source =
        "int lt(float a, float b, float c){ return (-a * (b / c)) < c; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested mul/div float relational compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_mixed_float_int_arithmetic_under_extension(void) {
    static const char *source =
        "float add(float x){ return x + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: mixed float/int arithmetic should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_call_int_arithmetic_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float add(float x){ return id(x) + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float-call/int arithmetic should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_literal_int_arithmetic_under_extension(void) {
    static const char *source =
        "float add(){ return 1.25 + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float-literal/int arithmetic should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_negative_float_call_int_arithmetic_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float add(float x){ return -id(x) * 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: negative float-call/int arithmetic should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_compare_against_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ return g == 1; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float compare against int should be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_nested_float_tree_plus_int_under_extension(void) {
    static const char *source =
        "float add(float x, float y){ return (x + y) + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested float tree plus int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_nested_float_muldiv_plus_int_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ return (-a * (b / c)) + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested float mul/div plus int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_plus_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return (g ? h : h) + 1; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value plus int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_plus_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float add(float x){ return (-id(x) ? x : x) + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value plus int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_plus_float_call_argument_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float wrap(float x){ return x; }\n"
        "float get(){ return wrap((g ? h : h) + h); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value plus float call argument should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return x; }\n"
        "float f(float x){ return wrap(((-id(x) ? x : x)) + x); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value plus float call argument should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_assignment_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ int x=0; x = (g ? h : h); return x; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value assignment to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_assignment_to_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "int main(){ int y=0; y = (-id(1.0) ? 1.0 : 2.0); return y; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value assignment to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_ternary_value_assignment_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ float y; y = (g ? h : h); return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value assignment to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_unary_call_float_ternary_value_assignment_to_float_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float f(float x){ float y; y = (-id(x) ? x : x); return y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call float ternary value assignment to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_chained_float_addition_assignment_to_float_under_extension(void) {
    static const char *source =
        "float f(float x, float y, float z){ float t; t = (x + y) + z; return t; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: chained float addition assignment to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_nested_float_mul_div_assignment_to_float_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ float t; t = -a * (b / c); return t; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested float mul/div assignment to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_ternary_value_initializer_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float y = (g ? h : h);\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value initializer to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_chained_float_addition_initializer_to_float_under_extension(void) {
    static const char *source =
        "float f(float x, float y, float z){ float t = (x + y) + z; return t; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: chained float addition initializer to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_nested_float_mul_div_initializer_to_float_under_extension(void) {
    static const char *source =
        "float f(float a, float b, float c){ float t = -a * (b / c); return t; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested float mul/div initializer to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_compare_against_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return (g ? h : h) == 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value compare against int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_compare_against_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "int main(){ return (-id(1.0) ? 1.0 : 2.0) == 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value compare against int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_compare_against_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int eq(){ return (g ? h : h) == h; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value compare against float should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_compare_against_float_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "int eq(float x){ return (-id(x) ? x : x) == x; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-035") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value compare against float should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_call_argument_to_int_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return sink((g ? h : h)); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value call argument to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_call_argument_to_int_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float id(float x){ return x; }\n"
        "int main(){ return sink((-id(1.0) ? 1.0 : 2.0)); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary value call argument to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_ternary_value_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((g ? h : h)); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value call argument to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_unary_call_ternary_value_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((-id(1.0) ? 1.0 : 2.0)); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call float ternary value call argument to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_chained_float_addition_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float wrap(float x){ return x; }\n"
        "float get(float x, float y, float z){ return wrap((x + y) + z); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: chained float addition call argument to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_nested_float_mul_div_call_argument_to_float_under_extension(void) {
    static const char *source =
        "float wrap(float x){ return x; }\n"
        "float f(float a, float b, float c){ return wrap(-a * (b / c)); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fdiv32") == NULL ||
        strstr(output, "__builtin_fmul32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: nested float mul/div call argument to float should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "float get(){ return pick() + h; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call arithmetic should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "float f(float x){ return pick(x) + x; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call arithmetic should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_float_helper_wrapped_ternary_call_compare_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int eq(){ return pick() == h; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int eq(float x){ return pick(x) == x; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call compare should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_helper_wrapped_ternary_call_plus_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "float add(){ return pick() + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call plus int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_helper_wrapped_ternary_call_plus_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "float add(float x){ return pick(x) + 1; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-008") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call plus int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_helper_wrapped_ternary_call_compare_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int eq(){ return pick() == 0; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call compare against int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_helper_wrapped_ternary_call_compare_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int eq(float x){ return pick(x) == 0; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-007") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call compare against int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_helper_wrapped_ternary_call_return_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int bad(){ return pick(); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call return to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_helper_wrapped_ternary_call_return_to_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int bad(float x){ return pick(x); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-005") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call return to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_helper_wrapped_ternary_call_initializer_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int x = pick();\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call initializer to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_helper_wrapped_ternary_call_initializer_to_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int y = pick(1.0);\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call initializer to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_helper_wrapped_ternary_call_assignment_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int main(){ int x = 0; x = pick(); return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call assignment to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_helper_wrapped_ternary_call_assignment_to_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int main(){ int y = 0; y = pick(1.0); return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-006") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call assignment to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_helper_wrapped_ternary_call_argument_to_int_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int main(){ return sink(pick()); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float helper-wrapped ternary call argument to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_helper_wrapped_ternary_call_argument_to_int_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int main(){ return sink(pick(1.0)); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-003") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call helper-wrapped ternary call argument to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_float_ternary_value_initializer_to_int_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int x = (g ? h : h);\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: float ternary value initializer to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_unary_call_ternary_value_initializer_to_int_under_extension(void) {
    static const char *source =
        "float id(float x){ return x; }\n"
        "int y = (-id(1.0) ? 1.0 : 2.0);\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-TYPE-004") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unary-call ternary float value initializer to int should still be rejected under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_float_conversion_on_direct_root_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ return int(g); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: direct-root explicit int(float) conversion should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_float_from_int_conversion_under_extension(void) {
    static const char *source =
        "float conv(int x, int y){ return float(x + y); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_i2f32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit float(int) conversion should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_redundant_explicit_same_type_conversion_for_now(void) {
    static const char *source =
        "float g = 1.25;\n"
        "int main(){ return float(g) == g; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-038") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: redundant explicit same-type conversion should still be rejected for now: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_float_conversion_under_extension(void) {
    static const char *source =
        "int conv(float x, float y){ return int(x + y); }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(float) conversion should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_float_ternary_bridge_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(float ternary) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension(void) {
    static const char *source =
        "int sink(int x){ return x; }\n"
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return sink(int(add3(1.0, 2.0, 3.0))); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(recursive float call) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int x = int(add3(1.0, 2.0, 3.0));\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__global.init") == NULL ||
        strstr(output, "__builtin_fadd32") == NULL ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(recursive float initializer) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ int x=0; x = int(add3(1.0, 2.0, 3.0)); return x; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(recursive float assignment) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_float_compare_bridge_under_extension(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h) == 2; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(float compare) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension(void) {
    static const char *source =
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return int(add3(1.0, 2.0, 3.0)) + 1; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_fadd32") == NULL ||
        strstr(output, "__builtin_f2i32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit int(recursive float arithmetic) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float z = float(add3(1, 2, 3));\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__global.init") == NULL ||
        strstr(output, "__builtin_i2f32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit float(recursive int initializer) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ float y; y = float(add3(1, 2, 3)); return y; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_i2f32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit float(recursive int assignment) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_float_from_int_compare_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ return float(add3(1, 2, 3)) == float(6); }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_i2f32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit float(int compare) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension(void) {
    static const char *source =
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ return float(add3(1, 2, 3)) + 1.25; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "__builtin_i2f32") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: explicit float(recursive int arithmetic) bridge should compile under extension: %s\n",
            error.message);
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int test_compiler_rejects_fndefer_outside_extension_mode(void) {
    static const char *source = "int main(){ fndefer putint(1); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error)) {
        fprintf(stderr, "[compiler] FAIL: fndefer should be rejected outside extension mode\n");
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_fndefer_under_extension(void) {
    static const char *source = "int main(){ int x=1; fndefer putint(x); x=2; return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension fndefer should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_fndefer_inside_conditional_registration_under_extension(void) {
    static const char *source = "int main(){ int x=1; if(x){ fndefer putint(x); } return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: conditional fndefer registration should compile under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_capdefer_under_extension(void) {
    static const char *source = "int main(){ int x=1; capdefer(y=x) putint(y); x=2; return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension capdefer should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_capdefer_inside_loop_registration_under_extension(void) {
    static const char *source = "int main(){ int x=1; for(;x<2;x=x+1){ capdefer(y=x) putint(y); } return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-EXT-034") == NULL) {
        fprintf(stderr, "[compiler] FAIL: loop capdefer registration should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_capdefer_multiple_bindings_under_extension(void) {
    static const char *source = "int main(){ int x=1; int z=2; capdefer(y=x, w=z) putint(y+w); x=3; z=4; return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension capdefer multi-binding should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_capdefer_outside_extension_mode(void) {
    static const char *source = "int main(){ int x=1; capdefer(y=x) putint(y); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        strstr(error.message, "SEMA-EXT-027") == NULL) {
        fprintf(stderr, "[compiler] FAIL: capdefer should be rejected outside extension mode: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_pair_parameter_under_extension_with_honest_diagnostic(void) {
    static const char *source = "pair id(pair x){ return x; }\nint main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "'pair' parameters/returns are not supported in this extension slice") == NULL) {
        fprintf(stderr, "[compiler] FAIL: pair parameter diagnostic mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_struct_parameter_under_extension_with_honest_diagnostic(void) {
    static const char *source =
        "struct Point { int x; int y; };\n"
        "struct Point id(struct Point x){ return x; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "'struct' parameters/returns are not supported in this extension slice") == NULL) {
        fprintf(stderr, "[compiler] FAIL: struct parameter diagnostic mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_pair_outside_extension_mode(void) {
    static const char *source = "int main(){ pair p={1,2}; return p.first; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        strstr(error.message, "SEMA-EXT-020") == NULL) {
        fprintf(stderr, "[compiler] FAIL: pair should be rejected outside extension mode: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_pair_under_extension(void) {
    static const char *source =
        "int main(){ pair p={3,4}; p.first = p.first + p.second; putint(p.first); return p.second; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension pair should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_pair_copy_under_extension(void) {
    static const char *source =
        "int main(){ pair a={1,2}; pair b=a; b=a; putint(b.first); return b.second; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension pair copy should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_pair_init_from_scalar_under_extension(void) {
    static const char *source = "int main(){ pair a=1; return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-005") == NULL) {
        fprintf(stderr, "[compiler] FAIL: pair scalar initialization should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_invalid_pair_field_under_extension(void) {
    static const char *source = "int main(){ pair p={1,2}; return p.third; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-003") == NULL) {
        fprintf(stderr, "[compiler] FAIL: invalid pair field should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_plain_pair_value_under_extension(void) {
    static const char *source = "int main(){ pair a={1,2}; return a; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-004") == NULL) {
        fprintf(stderr, "[compiler] FAIL: plain pair value should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_struct_copy_under_extension(void) {
    static const char *source =
        "struct Point { int x; int y; int z; };\n"
        "int main(){ struct Point a={1,2,3}; struct Point b=a; b=a; putint(b.x); putint(b.z); return b.y; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension struct copy should compile: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_struct_init_from_scalar_under_extension(void) {
    static const char *source =
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p=1; return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-005") == NULL) {
        fprintf(stderr, "[compiler] FAIL: struct scalar initialization should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_mismatched_struct_copy_init_under_extension(void) {
    static const char *source =
        "struct A { int x; int y; };\n"
        "struct B { int x; int y; };\n"
        "int main(){ struct B b={1,2}; struct A a=b; return a.x; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-006") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mismatched struct copy init should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_invalid_struct_field_under_extension(void) {
    static const char *source =
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p={1,2}; return p.z; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-003") == NULL) {
        fprintf(stderr, "[compiler] FAIL: invalid struct field should be rejected under extension: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_pair_struct_assignment_mismatch_under_extension(void) {
    static const char *source =
        "struct Point { int x; int y; };\n"
        "int main(){ pair a={1,2}; struct Point b={3,4}; a=b; return a.first; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        strstr(error.message, "SEMA-AGG-006") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: pair/struct assignment mismatch should be rejected under extension: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_function_valued_parameter_outside_extension_mode(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ return apply(add1, 41); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error)) {
        fprintf(stderr,
            "[compiler] FAIL: function-valued parameter syntax should be rejected outside extension mode\n");
        ok = 0;
    } else if (strstr(error.message, "SEMA-EXT-015") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: expected SEMA-EXT-015 for non-extension function-valued parameter, got: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_function_valued_parameter_extension_slice_for_now(void) {
    static const char *source =
        "int apply(int f(int), int x){ return f; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error)) {
        fprintf(stderr,
            "[compiler] FAIL: function-valued parameter extension slice should still reject before lowering\n");
        ok = 0;
    } else if (strstr(error.message, "SEMA-EXT-017") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: expected SEMA-EXT-017 for extension function-valued parameter direct use, got: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_unused_function_valued_parameter_declaration_under_extension(void) {
    static const char *source =
        "int apply(int f(int), int x){ return x; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl apply\n.type apply, @function\napply:\n") == NULL ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: unused function-valued parameter declaration should compile under extension: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_passing_top_level_function_value_to_unused_function_parameter_under_extension(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int apply(int f(int), int x){ return x; }\n"
        "int main(){ return apply(add1, 41); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl add1\n.type add1, @function\nadd1:\n") == NULL ||
        strstr(output, ".globl apply\n.type apply, @function\napply:\n") == NULL ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: passing top-level function value to unused parameter should compile in current extension slice: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_rejects_top_level_function_value_in_plain_value_position_under_extension_for_now(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int main(){ return add1; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error)) {
        fprintf(stderr,
            "[compiler] FAIL: plain top-level function value should still reject in current extension slice\n");
        ok = 0;
    } else if (strstr(error.message, "SEMA-EXT-019") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: expected SEMA-EXT-019 for plain top-level function value, got: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_direct_call_of_function_valued_parameter_with_specialization_lowering(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ return apply(add1, 41); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl apply__fv_0_add1\n.type apply__fv_0_add1, @function\napply__fv_0_add1:\n") == NULL ||
        strstr(output, "  call apply__fv_0_add1\n") == NULL ||
        strstr(output, "  call add1\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: direct call of function-valued parameter should compile via specialization lowering: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_direct_call_of_function_valued_parameter_with_multiple_specialized_bindings(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int double1(int x){ return x*2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ return apply(add1, 3) + apply(double1, 6); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl apply__fv_0_add1\n.type apply__fv_0_add1, @function\napply__fv_0_add1:\n") == NULL ||
        strstr(output, ".globl apply__fv_0_double1\n.type apply__fv_0_double1, @function\napply__fv_0_double1:\n") == NULL ||
        strstr(output, "  call apply__fv_0_add1\n") == NULL ||
        strstr(output, "  call apply__fv_0_double1\n") == NULL ||
        strstr(output, "  call add1\n") == NULL ||
        strstr(output, "  call double1\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: function-valued parameter specialization should support multiple top-level bindings: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_forwarding_function_valued_parameter_with_specialization_lowering(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int wrapper(int f(int), int x){ return apply(f, x); }\n"
        "int main(){ return wrapper(add1, 41); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl wrapper__fv_0_add1\n.type wrapper__fv_0_add1, @function\nwrapper__fv_0_add1:\n") == NULL ||
        strstr(output, ".globl apply__fv_0_add1\n.type apply__fv_0_add1, @function\napply__fv_0_add1:\n") == NULL ||
        strstr(output, "  call wrapper__fv_0_add1\n") == NULL ||
        strstr(output, "  call apply__fv_0_add1\n") == NULL ||
        strstr(output, "  call add1\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: function-valued parameter forwarding should compile via chained specialization lowering: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_multiple_function_valued_parameters_with_specialization_lowering(void) {
    static const char *source =
        "int add1(int x){ return x+1; }\n"
        "int double1(int x){ return x*2; }\n"
        "int compose(int f(int), int g(int), int x){ return f(g(x)); }\n"
        "int main(){ return compose(add1, double1, 20); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output,
            ".globl compose__fv_0_add1_1_double1\n.type compose__fv_0_add1_1_double1, @function\ncompose__fv_0_add1_1_double1:\n") == NULL ||
        strstr(output, "  call compose__fv_0_add1_1_double1\n") == NULL ||
        strstr(output, "  call double1\n") == NULL ||
        strstr(output, "  call add1\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: multiple function-valued parameters should compile via specialization lowering: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_void_function_valued_parameter_with_builtin_binding(void) {
    static const char *source =
        "void apply(void f(int), int x){ f(x); }\n"
        "int main(){ apply(putint, 7); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl apply__fv_0_putint\n.type apply__fv_0_putint, @function\napply__fv_0_putint:\n") == NULL ||
        strstr(output, "  call apply__fv_0_putint\n") == NULL ||
        strstr(output, "  call putint\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: void function-valued parameter should compile with builtin binding: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_zero_arg_function_valued_parameter_with_specialization_lowering(void) {
    static const char *source =
        "int next(){ return 9; }\n"
        "int apply0(int f()){ return f(); }\n"
        "int main(){ return apply0(next); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl apply0__fv_0_next\n.type apply0__fv_0_next, @function\napply0__fv_0_next:\n") == NULL ||
        strstr(output, "  call apply0__fv_0_next\n") == NULL ||
        strstr(output, "  call next\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: zero-arg function-valued parameter should compile with specialization lowering: %s\n",
            error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_accepts_zero_arg_void_function_valued_parameter_with_specialization_lowering(void) {
    static const char *source =
        "void ping(){ putint(1); }\n"
        "void apply0(void f()){ f(); }\n"
        "int main(){ apply0(ping); return 0; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, ".globl apply0__fv_0_ping\n.type apply0__fv_0_ping, @function\napply0__fv_0_ping:\n") == NULL ||
        strstr(output, "  call apply0__fv_0_ping\n") == NULL ||
        strstr(output, "  call ping\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: zero-arg void function-valued parameter should compile with specialization lowering: %s\n",
            error.message);
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

static int test_compiler_builds_extension_backend_dump_with_defer_return_order(void) {
    static const char *source =
        "int main(){ putint(1); defer putint(2); putint(3); return 0; }\n";
    static const long long expected_values[] = {1, 3, 2};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension defer compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension defer call ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_defer_lifo_order(void) {
    static const char *source =
        "int main(){ defer putint(1); defer putint(2); defer putint(3); return 0; }\n";
    static const long long expected_values[] = {3, 2, 1};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension defer LIFO compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension defer LIFO call ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_nested_defer_scope_order(void) {
    static const char *source =
        "int main(){ defer putint(1); { defer putint(2); } return 0; }\n";
    static const long long expected_values[] = {2, 1};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension nested defer compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension nested defer ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_loop_break_defer_order(void) {
    static const char *source =
        "int main(){ while(1){ defer putint(1); break; } return 0; }\n";
    static const long long expected_values[] = {1};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension loop-break defer compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension loop-break defer ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_nested_multi_defer_order(void) {
    static const char *source =
        "int main(){ defer putint(1); { defer putint(2); defer putint(3); } return 0; }\n";
    static const long long expected_values[] = {3, 2, 1};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension nested-multi defer compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension nested-multi defer ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_nested_defer_body_order(void) {
    static const char *source =
        "int main(){ defer { defer putint(1); { defer putint(2); } putint(3); } return 0; }\n";
    static const long long expected_values[] = {2, 3, 1};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension nested defer-body compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension nested defer-body ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_for_body_defer_before_step(void) {
    static const char *source =
        "int main(){ int i=0; for(;i<2;i=i+1){ defer putint(i); } return 0; }\n";
    CompilerError error;
    char *output = NULL;
    const char *call_site = NULL;
    const char *step_site = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension for-body defer compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    call_site = strstr(output, "  call putint\n");
    step_site = strstr(output, "  addi a0, a0, 1\n  sw a0, 0(sp)\n");
    if (!call_site || !step_site || call_site > step_site) {
        fprintf(stderr, "[compiler] FAIL: extension for-body defer ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_for_return_unwind_without_step(void) {
    static const char *source =
        "int main(){ defer putint(1); for(int i=0;i<3;i=i+1){ defer putint(2); return 0; } return 0; }\n";
    static const long long expected_values[] = {2, 1};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension for-return defer compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension for-return defer ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_fndefer_lifo_after_defer(void) {
    static const char *source =
        "int main(){ defer putint(1); fndefer putint(2); fndefer putint(3); return 0; }\n";
    static const long long expected_values[] = {1, 3, 2};
    CompilerError error;
    char *output = NULL;
    char *ret_line = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension fndefer LIFO compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    ret_line = strstr(output, "  ret\n");
    if (!ret_line ||
        !compiler_test_contains_putint_imm_sequence(
            output,
            expected_values,
            sizeof(expected_values) / sizeof(expected_values[0]))) {
        fprintf(stderr, "[compiler] FAIL: extension fndefer ordering mismatch: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_fndefer_return_value_frozen(void) {
    static const char *source =
        "int main(){ int x=1; fndefer x=2; return x; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) ||
        !output ||
        strstr(output, "  sw t6, 4(sp)\n") == NULL ||
        strstr(output, "  lw a0, 4(sp)\n") == NULL ||
        strstr(output, "  sw a0, 8(sp)\n") == NULL ||
        strstr(output, "  li t6, 2\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: extension fndefer return-freeze compile failed: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_builds_extension_backend_dump_with_global_init_call_without_stray_jalr(void) {
    static const char *source =
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "int main(){ return 0; }\n";
    CompilerError error;
    char *output = NULL;
    const char *call_site = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_EXTENSION, &output, &error) || !output) {
        fprintf(stderr, "[compiler] FAIL: extension global-init compile failed: %s\n", error.message);
        free(output);
        return 0;
    }

    call_site = strstr(output, "  call __global.init\n");
    if (!call_site ||
        strstr(call_site, "  jalr ra,") == call_site + strlen("  call __global.init\n")) {
        fprintf(stderr, "[compiler] FAIL: extension global-init call emitted stray jalr: %s\n", error.message);
        ok = 0;
    }

    free(output);
    return ok;
}


static int test_compiler_builds_riscv_backend_dump_for_sort_style_array_program(void) {
    static const char *source =
        "int n;\n"
        "int bubblesort(int arr[])\n"
        "{\n"
        "    int i;\n"
        "    int j;\n"
        "    i =0;\n"
        "    while(i < n-1){\n"
        "        j = 0;\n"
        "        while(j < n-i-1){\n"
        "            if (arr[j] > arr[j+1]) {\n"
        "                int tmp;\n"
        "                tmp = arr[j+1];\n"
        "                arr[j+1] = arr[j];\n"
        "                arr[j] = tmp;\n"
        "            }\n"
        "            j = j + 1;\n"
        "        }\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
        "int main(){\n"
        "    n = 10;\n"
        "    int a[10];\n"
        "    a[0]=4;a[1]=3;a[2]=9;a[3]=2;a[4]=0;\n"
        "    a[5]=1;a[6]=6;a[7]=5;a[8]=7;a[9]=8;\n"
        "    int i;\n"
        "    i = bubblesort(a);\n"
        "    while (i < n) {\n"
        "        int tmp;\n"
        "        tmp = a[i];\n"
        "        putint(tmp);\n"
        "        tmp = 10;\n"
        "        putch(tmp);\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, ".globl bubblesort\n.type bubblesort, @function\nbubblesort:\n") == NULL ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: sort-style riscv compile output mismatch: %s\n", error.message);
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
        strstr(output, "  li a0, 3\n") == NULL ||
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
        strstr(output, "  li a0, 1\n") == NULL ||
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
        strstr(output, "  addi a0, a0, 3\n") == NULL ||
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
        (strstr(output, "  call testeq\n") == NULL &&
            strstr(output, "  j testeq\n") == NULL) ||
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
        ((strstr(output, "  call set\n") == NULL || strstr(output, "  lw a0, %lo(g)(t5)\n") == NULL) &&
            strstr(output, "  li a0, 5\n") == NULL)) {
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
        strstr(output, "  ret\n") == NULL) {
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
        (strstr(output, "  addi a0, a0, 5\n") == NULL &&
            strstr(output, "  addi a1, a1, 5\n  mv a0, a1\n") == NULL)) {
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
int compiler_test_optimize_riscv_preview_reuse_repeated_lui_constants(char **io_text);
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

static int test_compiler_does_not_forward_store_copy_source_when_copy_reg_stays_live_after_store(void) {
    static const char *source_text =
        "  mv t4, a0\n"
        "  sw t4, 52(sp)\n"
        "  lw a0, 0(t5)\n"
        "  mv t6, t4\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: forward-store-copy-source liveness regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_forward_store_copy_source(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: store-copy source should stay unchanged when the copied reg remains live after the store\n");
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

static int test_compiler_does_not_fold_materialized_stack_slot_store_when_loaded_later(void) {
    static const char *source_text =
        "  addi t4, sp, 72\n"
        "  sw t4, 132(sp)\n"
        "  lw a2, 132(sp)\n"
        "  call concat\n"
        "  lw t6, 132(sp)\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: fold-materialized-stack-store-late-load regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_fold_materialized_stack_slot_accesses(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: materialized stack-slot address store should stay when the stack slot is loaded later\n");
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

static int test_compiler_folds_direct_call_tail_sequence(void) {
    static const char *source_text =
        "  call foo\n"
        "  lw s11, 4(sp)\n"
        "  lw ra, 8(sp)\n"
        "  addi sp, sp, 12\n"
        "  ret\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: direct-call tail regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_tail_calls(&text) ||
        !text ||
        strstr(text, "  call foo\n") != NULL ||
        strstr(text, "  j foo\n") == NULL ||
        strstr(text, "  lw s11, 4(sp)\n") == NULL ||
        strstr(text, "  lw ra, 8(sp)\n") == NULL ||
        strstr(text, "  addi sp, sp, 12\n") == NULL) {
        fprintf(stderr, "[compiler] FAIL: direct call tail sequence should fold into jump epilogue\n");
        ok = 0;
    }

    free(text);
    return ok;
}

static int test_compiler_does_not_fold_tail_call_when_arg_comes_from_current_frame(void) {
    static const char *source_text =
        "  addi a0, sp, 16\n"
        "  call foo\n"
        "  lw s11, 4(sp)\n"
        "  lw ra, 8(sp)\n"
        "  addi sp, sp, 12\n"
        "  ret\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: tail-call current-frame-arg regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_tail_calls(&text) ||
        !text ||
        strstr(text, source_text) == NULL) {
        fprintf(stderr, "[compiler] FAIL: tail-call fold should not cross a current-frame-derived argument\n");
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
        strstr(text, "70493\n") == NULL ||
        strstr(text, "-2031\n") == NULL ||
        strstr(text, "  mulh a0, t6, ") == NULL ||
        strstr(text, "  srli ") == NULL ||
        strstr(text, "  srai a0, a0, 26\n") == NULL ||
        strstr(text, "  add a0, a0, ") == NULL) {
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
        strstr(text, "  mulh a0, t6, ") == NULL ||
        strstr(text, "243712\n") == NULL ||
        strstr(text, "  sub a0, t6, a0\n") == NULL) {
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

static int test_compiler_rewrites_mod998_rem_before_ret_even_with_later_label(void) {
    static const char *source_text =
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a0, t6, t5\n"
        "  ret\n"
        ".Llater:\n"
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a1, t4, t5\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem ret+label regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mod998_divmods(&text) ||
        !text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem ret+label optimize failed\n");
        ok = 0;
        goto cleanup;
    }

    if (strstr(text, "  rem a0, t6, t5\n") != NULL ||
        strstr(text, "  rem a1, t4, t5\n") != NULL ||
        strstr(text, "  ret\n") == NULL ||
        strstr(text, ".Llater:\n") == NULL ||
        strstr(text, "  mulh a0, t6, ") == NULL ||
        strstr(text, "  mulh a1, t4, ") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mod998 rem rewrite should survive ret before later label\n");
        ok = 0;
    }

cleanup:
    free(text);
    return ok;
}

static int test_compiler_rewrites_mod998_rem_with_same_dest_and_source_when_temp_is_dead(void) {
    static const char *source_text =
        "  lui t5, 0x3b800\n"
        "  addi t5, t5, 1\n"
        "  rem a0, a0, t5\n"
        "  ret\n";
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(strlen(source_text) + 1u);
    if (!text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem same-rd-rs1 regression setup failed\n");
        return 0;
    }
    memcpy(text, source_text, strlen(source_text) + 1u);

    if (!compiler_test_optimize_riscv_preview_mod998_divmods(&text) ||
        !text) {
        fprintf(stderr, "[compiler] FAIL: mod998-rem same-rd-rs1 optimize failed\n");
        ok = 0;
        goto cleanup;
    }

    if (strstr(text, "  rem a0, a0, t5\n") != NULL ||
        strstr(text, "  mulh ") == NULL ||
        strstr(text, "  sub a0, a0, ") == NULL) {
        fprintf(stderr, "[compiler] FAIL: mod998 rem with same dest/source should rewrite when temp is dead\n");
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

static int test_compiler_does_not_tail_call_with_local_array_argument_in_text(void) {
    static const char *source =
        "int f(int a[], int n) { return a[0] + n; }\n"
        "int main(){ int a[10]; a[0] = 1; return f(a, 10); }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  j f\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: local-array call should not be folded into a tail jump: %s\n", error.message);
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

static int test_compiler_reuses_repeated_lui_constant_in_text(void) {
    char *text = NULL;
    int ok = 1;

    text = (char *)malloc(512u);
    if (!text) {
        return 0;
    }
    strcpy(text,
        ".Lhot:\n"
        "  lw t6, 28(sp)\n"
        "  lui t5, 0x1\n"
        "  add a0, t6, t5\n"
        "  lw t6, 32(sp)\n"
        "  lui t5, 0x1\n"
        "  add a1, t6, t5\n");

    if (!compiler_test_optimize_riscv_preview_reuse_repeated_lui_constants(&text) ||
        !text ||
        strstr(text,
            "  lw t6, 28(sp)\n"
            "  lui t5, 0x1\n"
            "  add a0, t6, t5\n"
            "  lw t6, 32(sp)\n"
            "  add a1, t6, t5\n") == NULL ||
        strstr(text,
            "  lw t6, 32(sp)\n"
            "  lui t5, 0x1\n"
            "  add a1, t6, t5\n") != NULL) {
        fprintf(stderr, "[compiler] FAIL: repeated lui constant materialization was not reused\n");
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

    if (strstr(output, "  call g12\n") == NULL ||
        strstr(output, "  call g15\n") == NULL ||
        strstr(output, "  call f\n") == NULL) {
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
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) || !output ||
        strstr(output, ".globl main\n.type main, @function\nmain:\n") == NULL ||
        strstr(output, "  li a0, 1\n") == NULL ||
        (strstr(output, "  blt a0, zero, .Lmain_3\n") == NULL &&
            strstr(output, "  blt a0, zero, .Lmain_2\n") == NULL &&
            strstr(output, "  li a0, 0\n") == NULL &&
            strstr(output, "  ret\n") == NULL)) {
        fprintf(stderr, "[compiler] FAIL: 32-bit wraparound loop-condition compile mismatch: %s\nactual:\n%s\n",
            error.message,
            output ? output : "<null>");
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
        strstr(output, "  li a0, 1\n") == NULL ||
        (strstr(output, "  blt a0, zero, .Lf_3\n") == NULL &&
            strstr(output, "  blt a0, zero, .Lf_2\n") == NULL &&
            strstr(output, "  li a0, 0\n") == NULL &&
            strstr(output, "  ret\n") == NULL)) {
        fprintf(stderr, "[compiler] FAIL: 32-bit wraparound condition runtime-path mismatch: %s\nactual:\n%s\n",
            error.message,
            output ? output : "<null>");
        ok = 0;
    }

    free(output);
    return ok;
}

static int test_compiler_preserves_assignment_condition_break_loop_exit(void) {
    static const char *source =
        "int g=0;"
        "int foo(){ g = g + 1; return 1; }"
        "int main(){ int b=1; while(1){ if(b = foo()) break; } return g; }\n";
    CompilerError error;
    char *output = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!compiler_compile_source_text(source, COMPILER_MODE_RISCV, &output, &error) ||
        !output ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  lw a0, %lo(g)(t5)\n") == NULL ||
        strstr(output, "  addi a0, a0, 1\n") == NULL ||
        strstr(output, "  sw a0, %lo(g)(t5)\n") == NULL ||
        strstr(output, "  li a0, 1\n") == NULL ||
        strstr(output, "  lui t5, %hi(g)\n") == NULL ||
        strstr(output, "  ret\n") == NULL) {
        fprintf(stderr,
            "[compiler] FAIL: assignment-condition break loop exit compile mismatch: %s\nactual:\n%s\n",
            error.message,
            output ? output : "<null>");
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
        (strstr(output, "  call f\n") == NULL &&
            strstr(output, "  j f\n") == NULL)) {
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
        strstr(output, "  li a0, 299\n") == NULL ||
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
        strstr(output, "  li a0, 0\n") == NULL ||
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
    const char *filter = getenv("COMPILER_DRIVER_REG_FILTER");
    int ok = 1;

    if (filter && filter[0] != '\0') {
        if (strstr("COMPILER-FLOAT-TRANSPORT", filter) != NULL) {
            return test_compiler_accepts_float_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-LITERAL-TRANSPORT", filter) != NULL) {
            return test_compiler_accepts_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-IDENT-INIT", filter) != NULL) {
            return test_compiler_accepts_float_global_initializer_transport_from_identifier_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-CALL-INIT", filter) != NULL) {
            return test_compiler_accepts_float_global_initializer_transport_from_call_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-RETURN-GLOBAL", filter) != NULL) {
            return test_compiler_accepts_float_return_transport_from_global_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-RETURN-GLOBAL-CALL", filter) != NULL) {
            return test_compiler_accepts_float_return_transport_from_global_call_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-PARAM-FORWARD", filter) != NULL) {
            return test_compiler_accepts_float_parameter_forward_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-PARAM-LOCAL-FORWARD", filter) != NULL) {
            return test_compiler_accepts_float_parameter_local_forward_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-CALL-CHAIN", filter) != NULL) {
            return test_compiler_accepts_float_global_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-LOCAL-CALL-CHAIN", filter) != NULL) {
            return test_compiler_accepts_float_local_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-IF-COND-ACCEPT", filter) != NULL) {
            return test_compiler_rejects_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_compiler_rejects_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-FOR-COND-ACCEPT", filter) != NULL) {
            return test_compiler_rejects_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-LOGICAL-COND-COMPOSE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_logical_condition_composition_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_same_type_float_ternary_value_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ARRAY-LOCAL-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_array_local_declaration_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ARRAY-GLOBAL-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_array_global_declaration_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ARRAY-PARAM-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_array_parameter_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-OP-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_operator_expression_with_global_literal_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-OP-INIT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_operator_expression_in_top_level_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-CALL-INIT-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_call_result_in_top_level_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ASSIGN-TRANSPORT", filter) != NULL) {
            return test_compiler_accepts_float_assignment_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ASSIGN-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-LITERAL-TRANSPORT", filter) != NULL) {
            return test_compiler_accepts_negative_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-IDENT-TRANSPORT", filter) != NULL) {
            return test_compiler_accepts_unary_minus_float_identifier_transport_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_negative_float_relational_compare_against_zero_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ADD-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-SUB-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_subtraction_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-ADD-COMBO-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_negative_float_addition_combo_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-SUB-COMBO-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_negative_float_subtraction_combo_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-MUL-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_multiplication_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-DIV-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_division_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-MUL-COMBO-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_negative_float_multiplication_combo_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-DIV-COMBO-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_negative_float_division_combo_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CHAIN-ADD-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_chained_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MUL-DIV-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_nested_float_mul_div_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CHAIN-ADD-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_chained_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CHAIN-ADD-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_chained_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MUL-DIV-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_nested_muldiv_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MUL-DIV-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_nested_muldiv_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_mixed_float_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_literal_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_negative_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-COMPARE-INT-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-TREE-PLUS-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_nested_float_tree_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_nested_float_muldiv_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_unary_call_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CHAIN-ADD-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_chained_float_addition_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MUL-DIV-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_nested_float_mul_div_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_ternary_value_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CHAIN-ADD-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_chained_float_addition_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MUL-DIV-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_nested_float_mul_div_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_unary_call_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-CHAIN-ADD-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_chained_float_addition_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-NESTED-MUL-DIV-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_nested_float_mul_div_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_float_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-PLUS-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_helper_wrapped_ternary_call_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-PLUS-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_helper_wrapped_ternary_call_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-COMPARE-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_helper_wrapped_ternary_call_compare_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_helper_wrapped_ternary_call_compare_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-RETURN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_helper_wrapped_ternary_call_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-RETURN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_helper_wrapped_ternary_call_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-INIT-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_helper_wrapped_ternary_call_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-INIT-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_helper_wrapped_ternary_call_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_helper_wrapped_ternary_call_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_helper_wrapped_ternary_call_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-HELPER-TERNARY-CALL-CALLARG-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_helper_wrapped_ternary_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-HELPER-TERNARY-CALL-CALLARG-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_helper_wrapped_ternary_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-TERNARY-VALUE-INIT-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_float_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT", filter) != NULL) {
            return test_compiler_rejects_unary_call_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-FLOAT-DIRECT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_float_conversion_on_direct_root_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_float_from_int_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-SAME-TYPE-REJECT", filter) != NULL) {
            return test_compiler_rejects_redundant_explicit_same_type_conversion_for_now() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-FLOAT-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_float_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-FLOAT-TERNARY-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_float_ternary_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-CALLARG-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-FLOAT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_float_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-FLOAT-FROM-INT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_float_from_int_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_compiler_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("COMPILER-FLOAT-GLOBAL-INIT", filter) != NULL) {
            return test_compiler_builds_extension_backend_dump_with_global_init_call_without_stray_jalr() ? 0 : 1;
        }
        fprintf(stderr, "[compiler] FAIL: unknown filter '%s'\n", filter);
        return 1;
    }

    ok &= test_compiler_parses_supported_modes();
    ok &= test_compiler_skips_all_paths_return_check_by_default();
    ok &= test_compiler_rejects_defer_outside_extension_mode();
    ok &= test_compiler_accepts_defer_under_extension_with_return_check_enabled();
    ok &= test_compiler_rejects_unless_outside_extension_mode();
    ok &= test_compiler_accepts_unless_under_extension();
    ok &= test_compiler_accepts_float_transport_under_extension();
    ok &= test_compiler_accepts_float_literal_transport_under_extension();
    ok &= test_compiler_accepts_float_global_initializer_transport_from_identifier_under_extension();
    ok &= test_compiler_accepts_float_global_initializer_transport_from_call_under_extension();
    ok &= test_compiler_accepts_float_return_transport_from_global_under_extension();
    ok &= test_compiler_accepts_float_return_transport_from_global_call_under_extension();
    ok &= test_compiler_accepts_float_parameter_forward_transport_under_extension();
    ok &= test_compiler_accepts_float_parameter_local_forward_transport_under_extension();
    ok &= test_compiler_accepts_float_global_call_chain_transport_under_extension();
    ok &= test_compiler_accepts_float_local_call_chain_transport_under_extension();
    ok &= test_compiler_rejects_float_if_condition_under_extension();
    ok &= test_compiler_rejects_float_while_condition_under_extension();
    ok &= test_compiler_rejects_float_for_condition_under_extension();
    ok &= test_compiler_accepts_float_logical_condition_composition_under_extension();
    ok &= test_compiler_rejects_float_array_local_declaration_under_extension();
    ok &= test_compiler_rejects_float_array_global_declaration_under_extension();
    ok &= test_compiler_rejects_float_array_parameter_under_extension();
    ok &= test_compiler_rejects_float_operator_expression_with_global_literal_under_extension();
    ok &= test_compiler_rejects_float_operator_expression_in_top_level_initializer_under_extension();
    ok &= test_compiler_rejects_float_call_result_in_top_level_initializer_under_extension();
    ok &= test_compiler_accepts_float_assignment_transport_under_extension();
    ok &= test_compiler_rejects_float_assignment_to_int_under_extension();
    ok &= test_compiler_accepts_float_equality_compare_under_extension();
    ok &= test_compiler_accepts_float_relational_compare_under_extension();
    ok &= test_compiler_accepts_negative_float_literal_transport_under_extension();
    ok &= test_compiler_accepts_unary_minus_float_identifier_transport_under_extension();
    ok &= test_compiler_accepts_negative_float_relational_compare_against_zero_under_extension();
    ok &= test_compiler_accepts_float_addition_under_extension();
    ok &= test_compiler_accepts_float_subtraction_under_extension();
    ok &= test_compiler_accepts_negative_float_addition_combo_under_extension();
    ok &= test_compiler_accepts_negative_float_subtraction_combo_under_extension();
    ok &= test_compiler_accepts_float_multiplication_under_extension();
    ok &= test_compiler_accepts_float_division_under_extension();
    ok &= test_compiler_accepts_negative_float_multiplication_combo_under_extension();
    ok &= test_compiler_accepts_negative_float_division_combo_under_extension();
    ok &= test_compiler_accepts_chained_float_addition_under_extension();
    ok &= test_compiler_accepts_nested_float_mul_div_under_extension();
    ok &= test_compiler_accepts_chained_float_equality_compare_under_extension();
    ok &= test_compiler_accepts_chained_float_relational_compare_under_extension();
    ok &= test_compiler_accepts_nested_muldiv_float_equality_compare_under_extension();
    ok &= test_compiler_accepts_nested_muldiv_float_relational_compare_under_extension();
    ok &= test_compiler_rejects_mixed_float_int_arithmetic_under_extension();
    ok &= test_compiler_rejects_float_call_int_arithmetic_under_extension();
    ok &= test_compiler_rejects_float_literal_int_arithmetic_under_extension();
    ok &= test_compiler_rejects_negative_float_call_int_arithmetic_under_extension();
    ok &= test_compiler_rejects_float_compare_against_int_under_extension();
    ok &= test_compiler_rejects_nested_float_tree_plus_int_under_extension();
    ok &= test_compiler_rejects_nested_float_muldiv_plus_int_under_extension();
    ok &= test_compiler_rejects_float_ternary_value_plus_int_under_extension();
    ok &= test_compiler_rejects_unary_call_ternary_value_plus_int_under_extension();
    ok &= test_compiler_rejects_float_ternary_value_plus_float_call_argument_under_extension();
    ok &= test_compiler_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension();
    ok &= test_compiler_rejects_float_ternary_value_assignment_to_int_under_extension();
    ok &= test_compiler_accepts_float_ternary_value_assignment_to_float_under_extension();
    ok &= test_compiler_accepts_unary_call_float_ternary_value_assignment_to_float_under_extension();
    ok &= test_compiler_accepts_chained_float_addition_assignment_to_float_under_extension();
    ok &= test_compiler_accepts_nested_float_mul_div_assignment_to_float_under_extension();
    ok &= test_compiler_accepts_float_ternary_value_initializer_to_float_under_extension();
    ok &= test_compiler_accepts_chained_float_addition_initializer_to_float_under_extension();
    ok &= test_compiler_accepts_nested_float_mul_div_initializer_to_float_under_extension();
    ok &= test_compiler_rejects_float_ternary_value_compare_against_int_under_extension();
    ok &= test_compiler_rejects_float_ternary_value_compare_against_float_under_extension();
    ok &= test_compiler_rejects_unary_call_ternary_value_compare_against_float_under_extension();
    ok &= test_compiler_rejects_float_ternary_value_call_argument_to_int_under_extension();
    ok &= test_compiler_rejects_unary_call_ternary_value_call_argument_to_int_under_extension();
    ok &= test_compiler_accepts_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_compiler_accepts_unary_call_ternary_value_call_argument_to_float_under_extension();
    ok &= test_compiler_accepts_chained_float_addition_call_argument_to_float_under_extension();
    ok &= test_compiler_accepts_nested_float_mul_div_call_argument_to_float_under_extension();
    ok &= test_compiler_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_compiler_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_compiler_accepts_float_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_compiler_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_compiler_rejects_float_helper_wrapped_ternary_call_plus_int_under_extension();
    ok &= test_compiler_rejects_unary_call_helper_wrapped_ternary_call_plus_int_under_extension();
    ok &= test_compiler_rejects_float_helper_wrapped_ternary_call_compare_int_under_extension();
    ok &= test_compiler_rejects_unary_call_helper_wrapped_ternary_call_compare_int_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_float_conversion_on_direct_root_under_extension();
    ok &= test_compiler_accepts_explicit_float_from_int_conversion_under_extension();
    ok &= test_compiler_rejects_redundant_explicit_same_type_conversion_for_now();
    ok &= test_compiler_accepts_explicit_int_from_float_conversion_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_float_ternary_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_float_compare_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_float_from_int_compare_bridge_under_extension();
    ok &= test_compiler_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension();
    ok &= test_compiler_accepts_same_type_float_ternary_value_under_extension();
    ok &= test_compiler_rejects_fndefer_outside_extension_mode();
    ok &= test_compiler_accepts_fndefer_under_extension();
    ok &= test_compiler_accepts_fndefer_inside_conditional_registration_under_extension();
    ok &= test_compiler_accepts_capdefer_under_extension();
    ok &= test_compiler_rejects_capdefer_inside_loop_registration_under_extension();
    ok &= test_compiler_accepts_capdefer_multiple_bindings_under_extension();
    ok &= test_compiler_rejects_capdefer_outside_extension_mode();
    ok &= test_compiler_rejects_pair_parameter_under_extension_with_honest_diagnostic();
    ok &= test_compiler_rejects_struct_parameter_under_extension_with_honest_diagnostic();
    ok &= test_compiler_rejects_pair_outside_extension_mode();
    ok &= test_compiler_accepts_pair_under_extension();
    ok &= test_compiler_accepts_pair_copy_under_extension();
    ok &= test_compiler_rejects_pair_init_from_scalar_under_extension();
    ok &= test_compiler_rejects_invalid_pair_field_under_extension();
    ok &= test_compiler_rejects_plain_pair_value_under_extension();
    ok &= test_compiler_accepts_struct_copy_under_extension();
    ok &= test_compiler_rejects_struct_init_from_scalar_under_extension();
    ok &= test_compiler_rejects_mismatched_struct_copy_init_under_extension();
    ok &= test_compiler_rejects_invalid_struct_field_under_extension();
    ok &= test_compiler_rejects_pair_struct_assignment_mismatch_under_extension();
    ok &= test_compiler_rejects_function_valued_parameter_outside_extension_mode();
    ok &= test_compiler_rejects_function_valued_parameter_extension_slice_for_now();
    ok &= test_compiler_accepts_unused_function_valued_parameter_declaration_under_extension();
    ok &= test_compiler_accepts_passing_top_level_function_value_to_unused_function_parameter_under_extension();
    ok &= test_compiler_rejects_top_level_function_value_in_plain_value_position_under_extension_for_now();
    ok &= test_compiler_accepts_direct_call_of_function_valued_parameter_with_specialization_lowering();
    ok &= test_compiler_accepts_direct_call_of_function_valued_parameter_with_multiple_specialized_bindings();
    ok &= test_compiler_accepts_forwarding_function_valued_parameter_with_specialization_lowering();
    ok &= test_compiler_accepts_multiple_function_valued_parameters_with_specialization_lowering();
    ok &= test_compiler_accepts_void_function_valued_parameter_with_builtin_binding();
    ok &= test_compiler_accepts_zero_arg_function_valued_parameter_with_specialization_lowering();
    ok &= test_compiler_accepts_zero_arg_void_function_valued_parameter_with_specialization_lowering();
    ok &= test_compiler_builds_riscv_backend_dump_from_source();
    ok &= test_compiler_builds_perf_backend_dump_from_source();
    ok &= test_compiler_builds_extension_backend_dump_with_defer_return_order();
    ok &= test_compiler_builds_extension_backend_dump_with_defer_lifo_order();
    ok &= test_compiler_builds_extension_backend_dump_with_nested_defer_scope_order();
    ok &= test_compiler_builds_extension_backend_dump_with_loop_break_defer_order();
    ok &= test_compiler_builds_extension_backend_dump_with_nested_multi_defer_order();
    ok &= test_compiler_builds_extension_backend_dump_with_nested_defer_body_order();
    ok &= test_compiler_builds_extension_backend_dump_with_for_body_defer_before_step();
    ok &= test_compiler_builds_extension_backend_dump_with_for_return_unwind_without_step();
    ok &= test_compiler_builds_extension_backend_dump_with_fndefer_lifo_after_defer();
    ok &= test_compiler_builds_extension_backend_dump_with_fndefer_return_value_frozen();
    ok &= test_compiler_builds_extension_backend_dump_with_global_init_call_without_stray_jalr();
    ok &= test_compiler_builds_riscv_backend_dump_for_sort_style_array_program();
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
    ok &= test_compiler_does_not_forward_store_copy_source_when_copy_reg_stays_live_after_store();
    ok &= test_compiler_removes_dead_jump_seed_move();
    ok &= test_compiler_does_not_remove_jump_seed_move_when_target_uses_it_first();
    ok &= test_compiler_folds_materialized_stack_slot_load();
    ok &= test_compiler_folds_materialized_stack_slot_store();
    ok &= test_compiler_folds_large_materialized_stack_slot_load();
    ok &= test_compiler_folds_large_materialized_stack_slot_store();
    ok &= test_compiler_does_not_fold_materialized_stack_slot_store_when_loaded_later();
    ok &= test_compiler_does_not_fold_tail_call_when_restore_instructions_separate_jal_and_epilogue();
    ok &= test_compiler_folds_direct_call_tail_sequence();
    ok &= test_compiler_does_not_fold_tail_call_when_arg_comes_from_current_frame();
    ok &= test_compiler_does_not_elide_zero_add_when_zero_reg_crosses_label();
    ok &= test_compiler_does_not_fold_mul_by_four_when_scale_reg_is_needed_past_label();
    ok &= test_compiler_does_not_fold_sub_by_one_when_one_reg_is_needed_past_label();
    ok &= test_compiler_rewrites_pow2_div_and_mod();
    ok &= test_compiler_rewrites_mod998_div_only();
    ok &= test_compiler_does_not_rewrite_mod998_div_when_t3_needed_past_label();
    ok &= test_compiler_rewrites_mod998_rem_only();
    ok &= test_compiler_does_not_rewrite_mod998_rem_when_t3_needed_past_label();
    ok &= test_compiler_rewrites_mod998_rem_before_ret_even_with_later_label();
    ok &= test_compiler_rewrites_mod998_rem_with_same_dest_and_source_when_temp_is_dead();
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
    ok &= test_compiler_does_not_tail_call_with_local_array_argument_in_text();
    ok &= test_compiler_reuses_repeated_lui_addi_constant_in_text();
    ok &= test_compiler_reuses_repeated_lui_constant_in_text();
    ok &= test_compiler_does_not_reuse_repeated_lui_addi_constant_across_call();
    ok &= test_compiler_handles_complex_const_shadowing_scopes();
    ok &= test_compiler_saves_caller_regs_around_whole_call_span();
    ok &= test_compiler_preserves_a0_across_nested_call_spans();
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
