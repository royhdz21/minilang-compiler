#ifndef TACVM_H
#define TACVM_H

#include <stdio.h>

typedef struct TacProgram TacProgram;

/* Loads a .tac file. Returns NULL and fills err on failure. */
TacProgram *tac_load(const char *path, char *err, size_t errsz);

/* Executes the program. read() takes integers from `in`, print writes to `out`.
 * max_steps <= 0 means unlimited. Returns 0 on success, -1 on a runtime error
 * (message in err). */
int tac_run(TacProgram *p, FILE *in, FILE *out, long max_steps, char *err, size_t errsz);

void tac_free(TacProgram *p);

#endif
