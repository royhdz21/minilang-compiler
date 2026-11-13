#include "parser/parser.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

/* Grammar (recursive descent, one token of lookahead):
 *
 *   program  := stmt* EOF
 *   stmt     := type ID (',' ID)* ';'
 *             | ID '=' expr ';'
 *             | 'if' '(' expr ')' stmt [ 'else' stmt ]
 *             | 'while' '(' expr ')' stmt
 *             | 'print' '(' expr ')' ';'
 *             | 'read' '(' ID ')' ';'
 *             | '{' stmt* '}'
 *   expr     := or
 *   or       := and ( '||' and )*
 *   and      := equality ( '&&' equality )*
 *   equality := relation ( ('==' | '!=') relation )*
 *   relation := additive ( ('<' | '<=' | '>' | '>=') additive )*
 *   additive := term ( ('+' | '-') term )*
 *   term     := unary ( ('*' | '/') unary )*
 *   unary    := ('!' | '-') unary | primary
 *   primary  := NUM | 'true' | 'false' | ID | '(' expr ')'
 */

typedef struct {
    Lexer lx;
    Token tok;      /* current lookahead */
    jmp_buf jb;     /* syntax errors abort the parse */
} Parser;

static void next(Parser *p) { p->tok = lexer_next(&p->lx); }

static void describe(const Token *t, char *buf, size_t n) {
    switch (t->type) {
    case TK_EOF: snprintf(buf, n, "end of file"); break;
    case TK_ID:  snprintf(buf, n, "identifier '%s'", t->text); break;
    case TK_NUM: snprintf(buf, n, "number '%s'", t->text); break;
    default:     snprintf(buf, n, "'%s'", t->text);
    }
}

static void syntax_error(Parser *p, const char *expected) {
    char d[96];
    describe(&p->tok, d, sizeof d);
    diag_error(p->tok.line, p->tok.col, "unexpected %s, expected %s", d, expected);
    longjmp(p->jb, 1);
}

static void expect(Parser *p, TokType t) {
    if (p->tok.type != t) {
        char e[16];
        snprintf(e, sizeof e, "'%s'", tok_name(t));
        syntax_error(p, e);
    }
    next(p);
}

static void *xcalloc(size_t n) {
    void *m = calloc(1, n);
    if (!m) { perror("calloc"); exit(2); }
    return m;
}

static Expr *mk_expr(ExprKind k, int line, int col) {
    Expr *e = xcalloc(sizeof *e);
    e->kind = k; e->line = line; e->col = col; e->type = T_ERR;
    return e;
}

static Expr *mk_bin(Token op, Expr *l, Expr *r) {
    Expr *e = mk_expr(EX_BINARY, op.line, op.col);
    e->op = op.type; e->lhs = l; e->rhs = r;
    return e;
}

static Expr *parse_expr(Parser *p);

static Expr *parse_primary(Parser *p) {
    Token t = p->tok;
    Expr *e;
    switch (t.type) {
    case TK_NUM:
        e = mk_expr(EX_NUM, t.line, t.col); e->value = t.value; next(p); return e;
    case KW_TRUE: case KW_FALSE:
        e = mk_expr(EX_BOOL, t.line, t.col); e->value = (t.type == KW_TRUE); next(p); return e;
    case TK_ID:
        e = mk_expr(EX_VAR, t.line, t.col); strcpy(e->name, t.text); next(p); return e;
    case TK_LPAREN:
        next(p); e = parse_expr(p); expect(p, TK_RPAREN); return e;
    default:
        syntax_error(p, "expression");
        return NULL;
    }
}

static Expr *parse_unary(Parser *p) {
    if (p->tok.type == TK_NOT || p->tok.type == TK_MINUS) {
        Token t = p->tok;
        next(p);
        Expr *e = mk_expr(EX_UNARY, t.line, t.col);
        e->op = t.type;
        e->lhs = parse_unary(p);
        return e;
    }
    return parse_primary(p);
}

static Expr *parse_term(Parser *p) {
    Expr *l = parse_unary(p);
    while (p->tok.type == TK_STAR || p->tok.type == TK_SLASH) {
        Token t = p->tok; next(p);
        l = mk_bin(t, l, parse_unary(p));
    }
    return l;
}

static Expr *parse_additive(Parser *p) {
    Expr *l = parse_term(p);
    while (p->tok.type == TK_PLUS || p->tok.type == TK_MINUS) {
        Token t = p->tok; next(p);
        l = mk_bin(t, l, parse_term(p));
    }
    return l;
}

static Expr *parse_relation(Parser *p) {
    Expr *l = parse_additive(p);
    while (p->tok.type == TK_LT || p->tok.type == TK_LE ||
           p->tok.type == TK_GT || p->tok.type == TK_GE) {
        Token t = p->tok; next(p);
        l = mk_bin(t, l, parse_additive(p));
    }
    return l;
}

static Expr *parse_equality(Parser *p) {
    Expr *l = parse_relation(p);
    while (p->tok.type == TK_EQ || p->tok.type == TK_NE) {
        Token t = p->tok; next(p);
        l = mk_bin(t, l, parse_relation(p));
    }
    return l;
}

static Expr *parse_and(Parser *p) {
    Expr *l = parse_equality(p);
    while (p->tok.type == TK_AND) {
        Token t = p->tok; next(p);
        l = mk_bin(t, l, parse_equality(p));
    }
    return l;
}

static Expr *parse_or(Parser *p) {
    Expr *l = parse_and(p);
    while (p->tok.type == TK_OR) {
        Token t = p->tok; next(p);
        l = mk_bin(t, l, parse_and(p));
    }
    return l;
}

static Expr *parse_expr(Parser *p) { return parse_or(p); }

/* ---- statements ---- */

static Stmt *mk_stmt(StmtKind k, const Token *t) {
    Stmt *s = xcalloc(sizeof *s);
    s->kind = k; s->line = t->line; s->col = t->col;
    return s;
}

static Stmt *parse_stmt(Parser *p);

static Stmt *parse_list(Parser *p, int in_block) {
    Stmt *head = NULL, *tail = NULL;
    while (p->tok.type != TK_EOF && !(in_block && p->tok.type == TK_RBRACE)) {
        Stmt *s = parse_stmt(p);              /* may be a chain (int a, b;) */
        if (!head) head = s; else tail->next = s;
        tail = s;
        while (tail->next) tail = tail->next;
    }
    return head;
}

/* Body of if/else/while: a lone declaration gets its own block scope. */
static Stmt *parse_body(Parser *p) {
    Stmt *s = parse_stmt(p);
    if (s->kind == ST_DECL) {
        Token t = { .line = s->line, .col = s->col };
        Stmt *b = mk_stmt(ST_BLOCK, &t);
        b->body = s;
        return b;
    }
    return s;
}

static void parse_cond(Parser *p, Stmt *s) {
    expect(p, TK_LPAREN);
    s->expr = parse_expr(p);
    expect(p, TK_RPAREN);
}

static Stmt *parse_stmt(Parser *p) {
    Token t = p->tok;
    Stmt *s;
    switch (t.type) {
    case KW_INT: case KW_BOOL: {
        Stmt *head = NULL, *tail = NULL;
        next(p);
        for (;;) {
            Token id = p->tok;
            if (id.type != TK_ID) syntax_error(p, "identifier");
            next(p);
            s = mk_stmt(ST_DECL, &id);
            s->decl_type = (t.type == KW_INT) ? T_INT : T_BOOL;
            strcpy(s->name, id.text);
            if (!head) head = s; else tail->next = s;
            tail = s;
            if (p->tok.type == TK_COMMA) { next(p); continue; }
            break;
        }
        expect(p, TK_SEMI);
        return head;
    }
    case TK_ID:
        s = mk_stmt(ST_ASSIGN, &t);
        strcpy(s->name, t.text);
        next(p);
        expect(p, TK_ASSIGN);
        s->expr = parse_expr(p);
        expect(p, TK_SEMI);
        return s;
    case KW_IF:
        s = mk_stmt(ST_IF, &t);
        next(p);
        parse_cond(p, s);
        s->body = parse_body(p);
        if (p->tok.type == KW_ELSE) { next(p); s->else_s = parse_body(p); }
        return s;
    case KW_WHILE:
        s = mk_stmt(ST_WHILE, &t);
        next(p);
        parse_cond(p, s);
        s->body = parse_body(p);
        return s;
    case KW_PRINT:
        s = mk_stmt(ST_PRINT, &t);
        next(p);
        parse_cond(p, s);                     /* '(' expr ')' */
        expect(p, TK_SEMI);
        return s;
    case KW_READ:
        s = mk_stmt(ST_READ, &t);
        next(p);
        expect(p, TK_LPAREN);
        if (p->tok.type != TK_ID) syntax_error(p, "identifier");
        strcpy(s->name, p->tok.text);
        next(p);
        expect(p, TK_RPAREN);
        expect(p, TK_SEMI);
        return s;
    case TK_LBRACE:
        s = mk_stmt(ST_BLOCK, &t);
        next(p);
        s->body = parse_list(p, 1);
        expect(p, TK_RBRACE);
        return s;
    default:
        syntax_error(p, "statement");
        return NULL;
    }
}

int parse_program(const char *src, Stmt **out) {
    Parser p;
    lexer_init(&p.lx, src);
    if (setjmp(p.jb)) { *out = NULL; return 0; }
    next(&p);
    *out = parse_list(&p, 0);
    return 1;
}
