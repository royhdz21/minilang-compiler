#include "semantics/semantics.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct Scope { Sym *syms; struct Scope *parent; } Scope;

static Scope *cur;
Sym *sem_symbols;
static Sym *all_tail;
static int depth;

static void *xcalloc(size_t n) {
    void *m = calloc(1, n);
    if (!m) { perror("calloc"); exit(2); }
    return m;
}

static void push_scope(void) {
    Scope *s = xcalloc(sizeof *s);
    s->parent = cur; cur = s; depth++;
}
static void pop_scope(void) { cur = cur->parent; depth--; }

static Sym *lookup_here(const char *n) {
    for (Sym *s = cur->syms; s; s = s->next_scope)
        if (!strcmp(s->name, n)) return s;
    return NULL;
}

static Sym *lookup(const char *n) {
    for (Scope *sc = cur; sc; sc = sc->parent)
        for (Sym *s = sc->syms; s; s = s->next_scope)
            if (!strcmp(s->name, n)) return s;
    return NULL;
}

/* Names like t1, t2, ... belong to compiler temporaries. */
static int temp_like(const char *n) {
    if (n[0] != 't' || !n[1]) return 0;
    for (int i = 1; n[i]; i++) if (!isdigit((unsigned char)n[i])) return 0;
    return 1;
}

static int unique_taken(const char *n) {
    if (temp_like(n)) return 1;
    for (Sym *s = sem_symbols; s; s = s->next_all)
        if (!strcmp(s->unique, n)) return 1;
    return 0;
}

static void declare(Stmt *s) {
    Sym *prev = lookup_here(s->name);
    if (prev) {
        diag_error(s->line, s->col, "redeclaration of '%s' (previously declared at line %d)",
                   s->name, prev->line);
        return;
    }
    Sym *v = xcalloc(sizeof *v);
    strcpy(v->name, s->name);
    v->type = s->decl_type; v->line = s->line; v->depth = depth;

    /* Storage name: the source name, or name_k if shadowed / clashing with a temporary. */
    char cand[NAMELEN]; int k = 0;
    strcpy(cand, s->name);
    while (unique_taken(cand)) snprintf(cand, sizeof cand, "%s_%d", s->name, ++k);
    strcpy(v->unique, cand);

    v->next_scope = cur->syms; cur->syms = v;
    if (!sem_symbols) sem_symbols = v; else all_tail->next_all = v;
    all_tail = v;
    s->sym = v;
}

static void check_operand(const Expr *op, Type got, Type want) {
    if (got != T_ERR && got != want)
        diag_error(op->line, op->col, "operator '%s' expects %s operands, got %s",
                   tok_name(op->op), type_name(want), type_name(got));
}

static Type sem_expr(Expr *e) {
    switch (e->kind) {
    case EX_NUM:  return e->type = T_INT;
    case EX_BOOL: return e->type = T_BOOL;
    case EX_VAR: {
        Sym *s = lookup(e->name);
        if (!s) {
            diag_error(e->line, e->col, "use of undeclared variable '%s'", e->name);
            return e->type = T_ERR;
        }
        e->sym = s;
        return e->type = s->type;
    }
    case EX_UNARY: {
        Type t = sem_expr(e->lhs);
        if (e->op == TK_NOT) { check_operand(e, t, T_BOOL); return e->type = T_BOOL; }
        check_operand(e, t, T_INT);
        return e->type = T_INT;
    }
    case EX_BINARY: {
        Type l = sem_expr(e->lhs), r = sem_expr(e->rhs);
        switch (e->op) {
        case TK_PLUS: case TK_MINUS: case TK_STAR: case TK_SLASH:
            check_operand(e, l, T_INT); check_operand(e, r, T_INT);
            return e->type = T_INT;
        case TK_LT: case TK_LE: case TK_GT: case TK_GE:
            check_operand(e, l, T_INT); check_operand(e, r, T_INT);
            return e->type = T_BOOL;
        case TK_EQ: case TK_NE:
            if (l != T_ERR && r != T_ERR && l != r)
                diag_error(e->line, e->col,
                           "operands of '%s' must have the same type (got %s and %s)",
                           tok_name(e->op), type_name(l), type_name(r));
            return e->type = T_BOOL;
        default: /* && || */
            check_operand(e, l, T_BOOL); check_operand(e, r, T_BOOL);
            return e->type = T_BOOL;
        }
    }
    }
    return T_ERR;
}

static void sem_stmt(Stmt *s);

static void sem_stmts(Stmt *s) { for (; s; s = s->next) sem_stmt(s); }

static void check_cond(const Stmt *s, const char *kw) {
    Type c = sem_expr(s->expr);
    if (c != T_ERR && c != T_BOOL)
        diag_error(s->expr->line, s->expr->col,
                   "condition of '%s' must be bool, got %s", kw, type_name(c));
}

static void sem_stmt(Stmt *s) {
    switch (s->kind) {
    case ST_DECL:
        declare(s);
        break;
    case ST_ASSIGN: {
        Sym *v = lookup(s->name);
        Type r = sem_expr(s->expr);
        if (!v) {
            diag_error(s->line, s->col, "assignment to undeclared variable '%s'", s->name);
            break;
        }
        s->sym = v;
        if (r != T_ERR && r != v->type)
            diag_error(s->line, s->col, "cannot assign %s expression to %s variable '%s'",
                       type_name(r), type_name(v->type), s->name);
        break;
    }
    case ST_IF:
        check_cond(s, "if");
        sem_stmt(s->body);
        if (s->else_s) sem_stmt(s->else_s);
        break;
    case ST_WHILE:
        check_cond(s, "while");
        sem_stmt(s->body);
        break;
    case ST_PRINT:
        sem_expr(s->expr);
        break;
    case ST_READ: {
        Sym *v = lookup(s->name);
        if (!v) { diag_error(s->line, s->col, "use of undeclared variable '%s'", s->name); break; }
        s->sym = v;
        if (v->type != T_INT)
            diag_error(s->line, s->col, "read() requires an int variable, '%s' is %s",
                       s->name, type_name(v->type));
        break;
    }
    case ST_BLOCK:
        push_scope();
        sem_stmts(s->body);
        pop_scope();
        break;
    }
}

int sem_analyze(Stmt *program) {
    int before = diag_count;
    cur = NULL; depth = -1;
    sem_symbols = NULL; all_tail = NULL;
    push_scope();                 /* global scope, depth 0 */
    sem_stmts(program);
    pop_scope();
    return diag_count - before;
}
