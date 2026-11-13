#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser/ast.h"

/* Emits Three-Address Code for a program that passed semantic analysis. */
void codegen(const Stmt *program, FILE *out);

#endif
