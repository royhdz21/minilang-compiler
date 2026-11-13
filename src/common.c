#include "common.h"
#include <stdarg.h>

const char *diag_file = "<input>";
int diag_count = 0;

const char *type_name(Type t) {
    switch (t) {
    case T_INT:  return "int";
    case T_BOOL: return "bool";
    default:     return "<error>";
    }
}

void diag_error(int line, int col, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d:%d: error: ", diag_file, line, col);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    diag_count++;
}
