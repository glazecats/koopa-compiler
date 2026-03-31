#if defined(__INTELLISENSE__) && !defined(PARSER_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(PARSER_SPLIT_AGGREGATOR)
#define PARSER_SPLIT_INTELLISENSE_CONTEXT
#include "parser.c"
#else

/*
 * Keep parser link-compatible without requiring src/ast/ast.c for legacy
 * parser-only builds.
 *
 * Maintenance checklist: when AstProgram/AstExternal/AstStatement/AstExpression
 * ownership fields change, update these parser_ast_* helpers together with
 * include/ast_internal.h lifecycle helpers to prevent legacy-link drift.
 */
#define AST_LIFECYCLE_STATIC static
#define AST_LIFECYCLE_STATEMENT_FREE_FN parser_ast_statement_free
#define AST_LIFECYCLE_PROGRAM_CLEAR_FN parser_ast_program_free
#define AST_LIFECYCLE_PROGRAM_ADD_EXTERNAL_FN parser_ast_program_add_external
#define AST_LIFECYCLE_EXPRESSION_FREE_FN parser_ast_expression_free
#include "ast_lifecycle_template.h"
#undef AST_LIFECYCLE_EXPRESSION_FREE_FN
#undef AST_LIFECYCLE_PROGRAM_ADD_EXTERNAL_FN
#undef AST_LIFECYCLE_PROGRAM_CLEAR_FN
#undef AST_LIFECYCLE_STATEMENT_FREE_FN
#undef AST_LIFECYCLE_STATIC

#endif
