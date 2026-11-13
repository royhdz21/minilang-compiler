#include "ir/codegen.h"
#include "semantics/semantics.h"
#include <stdarg.h>

/* TAC instruction set (see docs/language-spec.md):
 *   x := a            x := - a        x := ! a        x := a op b
 *   goto L            if a goto L     ifFalse a goto L
 *   if a relop b goto L
 *   L:                print a         printb a        read x       halt
 */

static FILE *out;
static int ntemp, nlabel;

typedef struct { char s[NAMELEN]; } Opd;   /* an operand: constant, variable or temporary */

static int new_temp(void)  { return ++ntemp; }
static int new_label(void) { return ++nlabel; }

static void emit(const char *fmt, ...) {
    va_list ap;
    fputs("    ", out);
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}
static void label(int n) { fprintf(out, "L%d:\n", n); }

static int is_rel(TokType op) {
    return op == TK_LT || op == TK_LE || op == TK_GT || op == TK_GE || op == TK_EQ || op == TK_NE;
}

static const char *negate_rel(TokType op) {
    switch (op) {
    case TK_LT: return ">=";
    case TK_LE: return ">";
    case TK_GT: return "<=";
    case TK_GE: return "<";
    case TK_EQ: return "!=";
    default:    return "==";
    }
}

/* Evaluates e and stores the name of the operand holding its value in *o. */
static void gen_expr(const Expr *e, Opd *o) {
    switch (e->kind) {
    case EX_NUM:
    case EX_BOOL:
        snprintf(o->s, sizeof o->s, "%d", e->value);     /* booleans are 0/1 */
        return;
    case EX_VAR:
        snprintf(o->s, sizeof o->s, "%s", e->sym->unique);
        return;
    case EX_UNARY: {
        Opd a;
        gen_expr(e->lhs, &a);
        int t = new_temp();
        emit("t%d := %s %s", t, tok_name(e->op), a.s);
        snprintf(o->s, sizeof o->s, "t%d", t);
        return;
    }
    case EX_BINARY:
        if (e->op == TK_AND || e->op == TK_OR) {         /* short-circuit evaluation */
            Opd a, b;
            gen_expr(e->lhs, &a);
            int t = new_temp(), end = new_label();
            emit("t%d := %s", t, a.s);
            if (e->op == TK_AND) emit("ifFalse t%d goto L%d", t, end);
            else                 emit("if t%d goto L%d", t, end);
            gen_expr(e->rhs, &b);
            emit("t%d := %s", t, b.s);
            label(end);
            snprintf(o->s, sizeof o->s, "t%d", t);
        } else {
            Opd a, b;
            gen_expr(e->lhs, &a);
            gen_expr(e->rhs, &b);
            int t = new_temp();
            emit("t%d := %s %s %s", t, a.s, tok_name(e->op), b.s);
            snprintf(o->s, sizeof o->s, "t%d", t);
        }
        return;
    }
}

/* Emits a jump to label L taken when the boolean expression e evaluates to
 * `when_true`; falls through otherwise. Conditions never materialize a value
 * unless they have to. */
static void gen_jump(const Expr *e, int when_true, int L) {
    if (e->kind == EX_UNARY && e->op == TK_NOT) {
        gen_jump(e->lhs, !when_true, L);
    } else if (e->kind == EX_BINARY && is_rel(e->op)) {
        Opd a, b;
        gen_expr(e->lhs, &a);
        gen_expr(e->rhs, &b);
        emit("if %s %s %s goto L%d", a.s, when_true ? tok_name(e->op) : negate_rel(e->op), b.s, L);
    } else if (e->kind == EX_BINARY && e->op == TK_AND) {
        if (when_true) {
            int skip = new_label();
            gen_jump(e->lhs, 0, skip);
            gen_jump(e->rhs, 1, L);
            label(skip);
        } else {
            gen_jump(e->lhs, 0, L);
            gen_jump(e->rhs, 0, L);
        }
    } else if (e->kind == EX_BINARY && e->op == TK_OR) {
        if (when_true) {
            gen_jump(e->lhs, 1, L);
            gen_jump(e->rhs, 1, L);
        } else {
            int skip = new_label();
            gen_jump(e->lhs, 1, skip);
            gen_jump(e->rhs, 0, L);
            label(skip);
        }
    } else {
        Opd a;
        gen_expr(e, &a);
        emit(when_true ? "if %s goto L%d" : "ifFalse %s goto L%d", a.s, L);
    }
}

static void gen_stmts(const Stmt *s);

static void gen_stmt(const Stmt *s) {
    switch (s->kind) {
    case ST_DECL:                       /* storage is implicit; see the symbol table header */
        break;
    case ST_ASSIGN: {
        Opd a;
        gen_expr(s->expr, &a);
        emit("%s := %s", s->sym->unique, a.s);
        break;
    }
    case ST_PRINT: {
        Opd a;
        gen_expr(s->expr, &a);
        emit("%s %s", s->expr->type == T_BOOL ? "printb" : "print", a.s);
        break;
    }
    case ST_READ:
        emit("read %s", s->sym->unique);
        break;
    case ST_BLOCK:
        gen_stmts(s->body);
        break;
    case ST_IF:
        if (s->else_s) {
            int l_then = new_label(), l_end = new_label();
            gen_jump(s->expr, 1, l_then);
            gen_stmt(s->else_s);
            emit("goto L%d", l_end);
            label(l_then);
            gen_stmt(s->body);
            label(l_end);
        } else {
            int l_end = new_label();
            gen_jump(s->expr, 0, l_end);
            gen_stmt(s->body);
            label(l_end);
        }
        break;
    case ST_WHILE: {
        int l_begin = new_label(), l_end = new_label();
        label(l_begin);
        gen_jump(s->expr, 0, l_end);
        gen_stmt(s->body);
        emit("goto L%d", l_begin);
        label(l_end);
        break;
    }
    }
}

static void gen_stmts(const Stmt *s) { for (; s; s = s->next) gen_stmt(s); }

void codegen(const Stmt *program, FILE *o) {
    out = o; ntemp = 0; nlabel = 0;
    fputs("# Mini-Lang Three-Address Code\n", out);
    fputs("# symbol table: storage name : type  (source name, declaration line)\n", out);
    for (const Sym *s = sem_symbols; s; s = s->next_all)
        fprintf(out, "#   %s : %s  (%s, line %d)\n", s->unique, type_name(s->type), s->name, s->line);
    gen_stmts(program);
    emit("halt");
}
