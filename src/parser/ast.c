#include "parser/ast.h"

static void ind(FILE *f, int d) { for (int i = 0; i < d; i++) fputs("  ", f); }

static void dump_expr(const Expr *e, FILE *f, int d) {
    ind(f, d);
    switch (e->kind) {
    case EX_NUM:  fprintf(f, "Num %d\n", e->value); break;
    case EX_BOOL: fprintf(f, "Bool %s\n", e->value ? "true" : "false"); break;
    case EX_VAR:  fprintf(f, "Var %s\n", e->name); break;
    case EX_UNARY:
        fprintf(f, "Unary %s\n", tok_name(e->op));
        dump_expr(e->lhs, f, d + 1);
        break;
    case EX_BINARY:
        fprintf(f, "Binary %s\n", tok_name(e->op));
        dump_expr(e->lhs, f, d + 1);
        dump_expr(e->rhs, f, d + 1);
        break;
    }
}

static void dump_stmts(const Stmt *s, FILE *f, int d) {
    for (; s; s = s->next) {
        ind(f, d);
        switch (s->kind) {
        case ST_DECL:   fprintf(f, "Decl %s %s\n", type_name(s->decl_type), s->name); break;
        case ST_ASSIGN: fprintf(f, "Assign %s\n", s->name); dump_expr(s->expr, f, d + 1); break;
        case ST_PRINT:  fprintf(f, "Print\n"); dump_expr(s->expr, f, d + 1); break;
        case ST_READ:   fprintf(f, "Read %s\n", s->name); break;
        case ST_BLOCK:  fprintf(f, "Block\n"); dump_stmts(s->body, f, d + 1); break;
        case ST_WHILE:
            fprintf(f, "While\n");
            dump_expr(s->expr, f, d + 1);
            dump_stmts(s->body, f, d + 1);
            break;
        case ST_IF:
            fprintf(f, "If\n");
            dump_expr(s->expr, f, d + 1);
            ind(f, d + 1); fputs("then:\n", f);
            dump_stmts(s->body, f, d + 2);
            if (s->else_s) { ind(f, d + 1); fputs("else:\n", f); dump_stmts(s->else_s, f, d + 2); }
            break;
        }
    }
}

void ast_dump(const Stmt *program, FILE *out) {
    fputs("Program\n", out);
    dump_stmts(program, out, 1);
}
