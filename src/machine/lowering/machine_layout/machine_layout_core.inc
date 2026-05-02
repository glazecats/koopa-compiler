#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

void machine_layout_program_init(MachineLayoutProgram *program) {
    if (!program) {
        return;
    }
    program->register_bank.register_count = 0;
    program->register_bank.registers = NULL;
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

void machine_layout_program_free(MachineLayoutProgram *program) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!program) {
        return;
    }
    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        machine_layout_register_desc_free(&program->register_bank.registers[register_index]);
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        machine_layout_global_free(&program->globals[global_index]);
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        machine_layout_function_free(&program->functions[function_index]);
    }
    free(program->register_bank.registers);
    free(program->globals);
    free(program->functions);
    machine_layout_program_init(program);
}

#endif
