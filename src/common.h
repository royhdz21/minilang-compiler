#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

#define NAMELEN 96

typedef enum { T_INT, T_BOOL, T_ERR } Type;

const char *type_name(Type t);

/* Diagnostics shared by every stage: "file:line:col: error: message" */
extern const char *diag_file;
extern int diag_count;
void diag_error(int line, int col, const char *fmt, ...);

#endif
