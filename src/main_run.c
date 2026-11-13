/* run: executes a .tac file with the TAC interpreter */
#include "runtime/tacvm.h"
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fputs("usage: run prog.tac [-i input.txt] [--max-steps N]\n"
          "  -i FILE          read() takes integers from FILE (default: standard input)\n"
          "  --max-steps N    abort after N executed instructions (default 50000000, 0 = unlimited)\n", stderr);
}

int main(int argc, char **argv) {
    const char *path = NULL, *inpath = NULL;
    long max_steps = 50000000L;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) inpath = argv[++i];
        else if (!strcmp(argv[i], "--max-steps") && i + 1 < argc) max_steps = atol(argv[++i]);
        else if (argv[i][0] == '-' && argv[i][1]) { usage(); return 2; }
        else path = argv[i];
    }
    if (!path) { usage(); return 2; }

    char err[256] = "";
    TacProgram *p = tac_load(path, err, sizeof err);
    if (!p) { fprintf(stderr, "run: %s\n", err); return 1; }

    FILE *in = stdin;
    if (inpath && !(in = fopen(inpath, "r"))) { fprintf(stderr, "run: cannot open '%s'\n", inpath); return 2; }

    int rc = tac_run(p, in, stdout, max_steps, err, sizeof err);
    fflush(stdout);
    if (rc) fprintf(stderr, "run: %s\n", err);
    tac_free(p);
    return rc ? 1 : 0;
}
