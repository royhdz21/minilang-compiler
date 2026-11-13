/* compile: Mini-Lang source (.src) -> Three-Address Code (.tac) */
#include "common.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantics/semantics.h"
#include "ir/codegen.h"
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    b[got] = 0;
    fclose(f);
    return b;
}

static void usage(void) {
    fputs("usage: compile input.src [-o out.tac] [--dump-tokens | --dump-ast]\n"
          "  -o FILE        write TAC to FILE (default: standard output)\n"
          "  --dump-tokens  stop after the scanner and print the token stream\n"
          "  --dump-ast     stop after the parser and print the syntax tree\n", stderr);
}

static const char *kind_of(TokType t) {
    if (t >= KW_INT && t <= KW_FALSE) return "KEYWORD";
    if (t == TK_ID) return "ID";
    if (t == TK_NUM) return "NUM";
    return "SYMBOL";
}

int main(int argc, char **argv) {
    const char *in = NULL, *outp = NULL;
    int dump_tokens = 0, dump_ast = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) outp = argv[++i];
        else if (!strcmp(argv[i], "--dump-tokens")) dump_tokens = 1;
        else if (!strcmp(argv[i], "--dump-ast")) dump_ast = 1;
        else if (argv[i][0] == '-' && argv[i][1]) { usage(); return 2; }
        else in = argv[i];
    }
    if (!in) { usage(); return 2; }

    char *src = read_file(in);
    if (!src) { fprintf(stderr, "compile: cannot read '%s'\n", in); return 2; }
    diag_file = in;

    /* Stage 1: scanner */
    if (dump_tokens) {
        Lexer lx; lexer_init(&lx, src);
        for (;;) {
            Token t = lexer_next(&lx);
            printf("%d:%d\t%s\t%s\n", t.line, t.col, kind_of(t.type), t.text);
            if (t.type == TK_EOF) break;
        }
        return diag_count ? 1 : 0;
    }

    /* Stage 2: parser */
    Stmt *prog = NULL;
    int ok = parse_program(src, &prog);
    if (!ok || diag_count) {
        fprintf(stderr, "%d error(s); compilation aborted\n", diag_count);
        return 1;
    }
    if (dump_ast) { ast_dump(prog, stdout); return 0; }

    /* Stage 3: semantic analysis */
    if (sem_analyze(prog)) {
        fprintf(stderr, "%d error(s); compilation aborted\n", diag_count);
        return 1;
    }

    /* Stage 4: code generation */
    FILE *out = stdout;
    if (outp) {
        out = fopen(outp, "w");
        if (!out) { fprintf(stderr, "compile: cannot write '%s'\n", outp); return 2; }
    }
    codegen(prog, out);
    if (outp) fclose(out);
    return 0;
}
