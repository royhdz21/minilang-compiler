#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

/* NOTE: keep in the same order as NAMES[] in lexer.c */
typedef enum {
    TK_EOF,
    KW_INT, KW_BOOL, KW_IF, KW_ELSE, KW_WHILE, KW_PRINT, KW_READ, KW_TRUE, KW_FALSE,
    TK_ID, TK_NUM,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH,
    TK_LT, TK_LE, TK_GT, TK_GE, TK_EQ, TK_NE, TK_ASSIGN,
    TK_AND, TK_OR, TK_NOT,
    TK_SEMI, TK_COMMA, TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE
} TokType;

typedef struct {
    TokType type;
    char text[64];   /* lexeme (operators/keywords: their spelling) */
    int value;       /* numeric value for TK_NUM */
    int line, col;   /* 1-based position of the first character */
} Token;

typedef struct {
    const char *src;
    size_t pos;
    int line, col;
} Lexer;

void lexer_init(Lexer *l, const char *src);
Token lexer_next(Lexer *l);            /* returns TK_EOF forever at the end */
const char *tok_name(TokType t);       /* spelling, e.g. "<=" or "while" */

#endif
