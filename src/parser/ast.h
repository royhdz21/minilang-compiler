#ifndef AST_H
#define AST_H

#include "common.h"
#include "lexer/lexer.h"

typedef struct Sym Sym;   /* defined in semantics/semantics.h */

typedef enum { EX_NUM, EX_BOOL, EX_VAR, EX_UNARY, EX_BINARY } ExprKind;

typedef struct Expr {
    ExprKind kind;
    int line, col;          /* operators: position of the operator token */
    TokType op;             /* EX_UNARY / EX_BINARY */
    int value;              /* EX_NUM value, EX_BOOL 0/1 */
    char name[64];          /* EX_VAR */
    Sym *sym;               /* EX_VAR: filled by the semantic analyzer */
    struct Expr *lhs, *rhs; /* operands (unary uses lhs only) */
    Type type;              /* filled by the semantic analyzer */
} Expr;

typedef enum { ST_DECL, ST_ASSIGN, ST_IF, ST_WHILE, ST_PRINT, ST_READ, ST_BLOCK } StmtKind;

typedef struct Stmt {
    StmtKind kind;
    int line, col;
    Type decl_type;         /* ST_DECL */
    char name[64];          /* ST_DECL / ST_ASSIGN / ST_READ target */
    Sym *sym;               /* filled by the semantic analyzer */
    Expr *expr;             /* condition, assigned value or printed value */
    struct Stmt *body;      /* if: then-branch, while: body, block: first statement */
    struct Stmt *else_s;    /* if: optional else-branch */
    struct Stmt *next;      /* next statement in the enclosing list */
} Stmt;

void ast_dump(const Stmt *program, FILE *out);

#endif
