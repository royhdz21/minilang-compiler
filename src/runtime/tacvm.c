#include "runtime/tacvm.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define NAMELEN 96
#define MAXTOK 8

typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE,
               OP_AND, OP_OR, OP_NEG, OP_NOT } AluOp;
typedef enum { I_COPY, I_BIN, I_UN, I_GOTO, I_IF, I_IFNOT, I_IFREL,
               I_PRINT, I_PRINTB, I_READ, I_HALT } Kind;

typedef struct { int is_const, val, slot; } Opd;   /* literal or variable slot */

typedef struct {
    Kind kind;
    AluOp op;
    Opd a, b;
    int dst;                 /* variable slot written */
    int target;              /* resolved jump target (instruction index) */
    int srcline;             /* line in the .tac file, for diagnostics */
    char tlabel[NAMELEN];    /* jump label before resolution */
} Instr;

struct TacProgram {
    Instr *code; int n, cap;
    char (*labels)[NAMELEN]; int *label_at; int nl, lcap;
    char (*vars)[NAMELEN]; int nv, vcap;
};

static void *xrealloc(void *p, size_t n) {
    void *m = realloc(p, n);
    if (!m) { perror("realloc"); exit(2); }
    return m;
}

static int intern(TacProgram *p, const char *name) {
    for (int i = 0; i < p->nv; i++) if (!strcmp(p->vars[i], name)) return i;
    if (p->nv == p->vcap) {
        p->vcap = p->vcap ? p->vcap * 2 : 64;
        p->vars = xrealloc(p->vars, (size_t)p->vcap * sizeof *p->vars);
    }
    snprintf(p->vars[p->nv], NAMELEN, "%s", name);
    return p->nv++;
}

static int is_ident(const char *s) {
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_')) return 0;
    return 1;
}

static int parse_opd(TacProgram *p, const char *s, Opd *o) {
    if (isdigit((unsigned char)s[0]) || (s[0] == '-' && isdigit((unsigned char)s[1]))) {
        char *end; long v = strtol(s, &end, 10);
        if (*end || v > INT_MAX || v < INT_MIN) return 0;
        o->is_const = 1; o->val = (int)v;
        return 1;
    }
    if (!is_ident(s)) return 0;
    o->is_const = 0; o->slot = intern(p, s);
    return 1;
}

static int alu_from(const char *s) {
    static const char *names[] = { "+", "-", "*", "/", "<", "<=", ">", ">=", "==", "!=", "&&", "||" };
    for (int i = 0; i < 12; i++) if (!strcmp(s, names[i])) return i;
    return -1;
}

static int split(const char *s, char (*tk)[NAMELEN], int max) {
    int n = 0;
    while (*s) {
        while (isspace((unsigned char)*s)) s++;
        if (!*s) break;
        if (n == max) return -1;
        int k = 0;
        while (*s && !isspace((unsigned char)*s)) { if (k < NAMELEN - 1) tk[n][k++] = *s; s++; }
        tk[n][k] = 0; n++;
    }
    return n;
}

static int add_label(TacProgram *p, const char *name, int at, int ln, char *err, size_t en) {
    for (int i = 0; i < p->nl; i++)
        if (!strcmp(p->labels[i], name)) {
            snprintf(err, en, "line %d: duplicate label '%s'", ln, name);
            return 0;
        }
    if (p->nl == p->lcap) {
        p->lcap = p->lcap ? p->lcap * 2 : 32;
        p->labels = xrealloc(p->labels, (size_t)p->lcap * sizeof *p->labels);
        p->label_at = xrealloc(p->label_at, (size_t)p->lcap * sizeof *p->label_at);
    }
    snprintf(p->labels[p->nl], NAMELEN, "%s", name);
    p->label_at[p->nl++] = at;
    return 1;
}

#define BAD() do { snprintf(err, en, "line %d: malformed instruction", ln); return 0; } while (0)

static int parse_instr(TacProgram *p, char (*t)[NAMELEN], int n, int ln, char *err, size_t en) {
    Instr in;
    memset(&in, 0, sizeof in);
    in.srcline = ln;

    if (!strcmp(t[0], "goto")) {
        if (n != 2) BAD();
        in.kind = I_GOTO; snprintf(in.tlabel, NAMELEN, "%s", t[1]);
    } else if (!strcmp(t[0], "if") || !strcmp(t[0], "ifFalse")) {
        int neg = (t[0][2] == 'F');
        if (n == 4 && !strcmp(t[2], "goto")) {
            in.kind = neg ? I_IFNOT : I_IF;
            if (!parse_opd(p, t[1], &in.a)) BAD();
            snprintf(in.tlabel, NAMELEN, "%s", t[3]);
        } else if (n == 6 && !neg && !strcmp(t[4], "goto")) {
            int op = alu_from(t[2]);
            if (op < OP_LT || op > OP_NE) BAD();
            in.kind = I_IFREL; in.op = (AluOp)op;
            if (!parse_opd(p, t[1], &in.a) || !parse_opd(p, t[3], &in.b)) BAD();
            snprintf(in.tlabel, NAMELEN, "%s", t[5]);
        } else BAD();
    } else if (!strcmp(t[0], "print") || !strcmp(t[0], "printb")) {
        if (n != 2) BAD();
        in.kind = t[0][5] ? I_PRINTB : I_PRINT;
        if (!parse_opd(p, t[1], &in.a)) BAD();
    } else if (!strcmp(t[0], "read")) {
        if (n != 2 || !is_ident(t[1])) BAD();
        in.kind = I_READ; in.dst = intern(p, t[1]);
    } else if (!strcmp(t[0], "halt")) {
        if (n != 1) BAD();
        in.kind = I_HALT;
    } else if (n >= 3 && !strcmp(t[1], ":=") && is_ident(t[0])) {
        in.dst = intern(p, t[0]);
        if (n == 3) {
            in.kind = I_COPY;
            if (!parse_opd(p, t[2], &in.a)) BAD();
        } else if (n == 4) {
            in.kind = I_UN;
            if (!strcmp(t[2], "-")) in.op = OP_NEG;
            else if (!strcmp(t[2], "!")) in.op = OP_NOT;
            else BAD();
            if (!parse_opd(p, t[3], &in.a)) BAD();
        } else if (n == 5) {
            int op = alu_from(t[3]);
            if (op < 0) BAD();
            in.kind = I_BIN; in.op = (AluOp)op;
            if (!parse_opd(p, t[2], &in.a) || !parse_opd(p, t[4], &in.b)) BAD();
        } else BAD();
    } else BAD();

    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 64;
        p->code = xrealloc(p->code, (size_t)p->cap * sizeof *p->code);
    }
    p->code[p->n++] = in;
    return 1;
}

TacProgram *tac_load(const char *path, char *err, size_t en) {
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(err, en, "cannot open '%s'", path); return NULL; }
    TacProgram *p = calloc(1, sizeof *p);
    if (!p) { perror("calloc"); exit(2); }
    char line[1024];
    int ln = 0;

    while (fgets(line, sizeof line, f)) {
        ln++;
        char *h = strchr(line, '#');
        if (h) *h = 0;
        char tk[MAXTOK][NAMELEN];
        int n = split(line, tk, MAXTOK), s = 0;
        if (n < 0) { snprintf(err, en, "line %d: too many tokens", ln); goto fail; }
        if (n > 0) {
            size_t len = strlen(tk[0]);
            if (tk[0][len - 1] == ':' && len > 1) {          /* "L1:" optionally followed by an instruction */
                tk[0][len - 1] = 0;
                if (!add_label(p, tk[0], p->n, ln, err, en)) goto fail;
                s = 1;
            }
        }
        if (n - s == 0) continue;
        if (!parse_instr(p, tk + s, n - s, ln, err, en)) goto fail;
    }
    fclose(f);

    for (int i = 0; i < p->n; i++) {                           /* resolve jump targets */
        Instr *in = &p->code[i];
        if (in->kind != I_GOTO && in->kind != I_IF && in->kind != I_IFNOT && in->kind != I_IFREL) continue;
        int found = 0;
        for (int k = 0; k < p->nl; k++)
            if (!strcmp(p->labels[k], in->tlabel)) { in->target = p->label_at[k]; found = 1; break; }
        if (!found) {
            snprintf(err, en, "line %d: undefined label '%s'", in->srcline, in->tlabel);
            tac_free(p);
            return NULL;
        }
    }
    return p;

fail:
    fclose(f);
    tac_free(p);
    return NULL;
}

void tac_free(TacProgram *p) {
    if (!p) return;
    free(p->code); free(p->labels); free(p->label_at); free(p->vars); free(p);
}

static int alu(AluOp op, int a, int b, int *ok) {
    switch (op) {
    case OP_ADD: return (int)((unsigned)a + (unsigned)b);
    case OP_SUB: return (int)((unsigned)a - (unsigned)b);
    case OP_MUL: return (int)((unsigned)a * (unsigned)b);
    case OP_DIV:
        if (b == 0) { *ok = 0; return 0; }
        if (a == INT_MIN && b == -1) return INT_MIN;
        return a / b;
    case OP_LT:  return a < b;
    case OP_LE:  return a <= b;
    case OP_GT:  return a > b;
    case OP_GE:  return a >= b;
    case OP_EQ:  return a == b;
    case OP_NE:  return a != b;
    case OP_AND: return a && b;
    case OP_OR:  return a || b;
    default:     return 0;
    }
}

#define V(o) ((o).is_const ? (o).val : v[(o).slot])

int tac_run(TacProgram *p, FILE *in, FILE *out, long max_steps, char *err, size_t en) {
    int *v = calloc(p->nv ? (size_t)p->nv : 1, sizeof *v);    /* every variable starts at 0 */
    if (!v) { perror("calloc"); exit(2); }
    long steps = 0;
    int pc = 0, rc = 0;

    while (pc < p->n) {
        const Instr *I = &p->code[pc++];
        if (max_steps > 0 && ++steps > max_steps) {
            snprintf(err, en, "step limit (%ld) exceeded; possible infinite loop", max_steps);
            rc = -1; break;
        }
        int ok = 1;
        switch (I->kind) {
        case I_COPY:  v[I->dst] = V(I->a); break;
        case I_BIN:
            v[I->dst] = alu(I->op, V(I->a), V(I->b), &ok);
            if (!ok) {
                snprintf(err, en, "runtime error at TAC line %d: division by zero", I->srcline);
                rc = -1; goto done;
            }
            break;
        case I_UN: {
            int x = V(I->a);
            v[I->dst] = (I->op == OP_NEG) ? (int)(0u - (unsigned)x) : (x == 0);
            break;
        }
        case I_GOTO:  pc = I->target; break;
        case I_IF:    if (V(I->a))  pc = I->target; break;
        case I_IFNOT: if (!V(I->a)) pc = I->target; break;
        case I_IFREL: if (alu(I->op, V(I->a), V(I->b), &ok)) pc = I->target; break;
        case I_PRINT:  fprintf(out, "%d\n", V(I->a)); break;
        case I_PRINTB: fputs(V(I->a) ? "true\n" : "false\n", out); break;
        case I_READ: {
            int x;
            if (fscanf(in, "%d", &x) != 1) {
                snprintf(err, en, "runtime error at TAC line %d: read() found no integer on input", I->srcline);
                rc = -1; goto done;
            }
            v[I->dst] = x;
            break;
        }
        case I_HALT: pc = p->n; break;
        }
    }
done:
    free(v);
    return rc;
}
