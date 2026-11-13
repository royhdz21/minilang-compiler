#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "parser/ast.h"

/* One entry of the symbol table (one per declaration). */
struct Sym {
    char name[NAMELEN];     /* name in the source */
    char unique[NAMELEN];   /* storage name used by the generated TAC */
    Type type;
    int line;               /* declaration line */
    int depth;              /* scope depth (0 = global) */
    Sym *next_all;          /* all symbols, in declaration order */
    Sym *next_scope;        /* symbols of the same scope */
};

extern Sym *sem_symbols;    /* head of the list of every declared symbol */

/* Annotates the AST (types, symbol links) and reports errors with diag_error().
 * Returns the number of semantic errors found. */
int sem_analyze(Stmt *program);

#endif
