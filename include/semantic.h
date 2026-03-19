#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

typedef struct {
    int line;
    int column;
    char message[128];
} SemanticError;

/*
 * Performs semantic checks on parsed top-level AST nodes.
 * Returns 1 on success, 0 on semantic error.
 */
int semantic_analyze_program(const AstProgram *program, SemanticError *error);

#endif