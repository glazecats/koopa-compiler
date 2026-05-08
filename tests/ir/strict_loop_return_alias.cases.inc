static int test_ir_lowers_nested_loop_alias_return_family_without_malformed_exit_block(void) {
    return expect_ir_lower_succeeds("IR-NESTED-LOOP-ALIAS-RETURN-FAMILY",
        "int f(int a){while(1){if(a){return 1;}int b=a;b=1;for(;b<2;b=b+1){if(b){return 2;}}}}\n");
}
