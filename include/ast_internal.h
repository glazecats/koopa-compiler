#ifndef AST_INTERNAL_H
#define AST_INTERNAL_H

#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* Internal lifecycle helpers are instantiated from a shared template to keep
 * parser compatibility helpers and AST core helpers in lock-step.
 */
#define AST_LIFECYCLE_STATIC static inline
#define AST_LIFECYCLE_STATEMENT_FREE_FN ast_statement_free_internal
#define AST_LIFECYCLE_PROGRAM_CLEAR_FN ast_program_clear_storage
#define AST_LIFECYCLE_PROGRAM_ADD_EXTERNAL_FN ast_program_append_external
#define AST_LIFECYCLE_EXPRESSION_FREE_FN ast_expression_free_internal
#include "ast_lifecycle_template.h"
#undef AST_LIFECYCLE_EXPRESSION_FREE_FN
#undef AST_LIFECYCLE_PROGRAM_ADD_EXTERNAL_FN
#undef AST_LIFECYCLE_PROGRAM_CLEAR_FN
#undef AST_LIFECYCLE_STATEMENT_FREE_FN
#undef AST_LIFECYCLE_STATIC

#endif