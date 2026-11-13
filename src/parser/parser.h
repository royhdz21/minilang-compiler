#ifndef PARSER_H
#define PARSER_H

#include "parser/ast.h"

/* Parses a whole program. Returns 1 on success (*out may be NULL for an empty
 * program) and 0 after reporting a syntax error through diag_error(). */
int parse_program(const char *src, Stmt **out);

#endif
