#include "lexer/lexer.h"
#include "common.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>

static const char *NAMES[] = {
    "<eof>",
    "int", "bool", "if", "else", "while", "print", "read", "true", "false",
    "identifier", "number",
    "+", "-", "*", "/",
    "<", "<=", ">", ">=", "==", "!=", "=",
    "&&", "||", "!",
    ";", ",", "(", ")", "{", "}"
};

const char *tok_name(TokType t) { return NAMES[t]; }

void lexer_init(Lexer *l, const char *src) {
    l->src = src; l->pos = 0; l->line = 1; l->col = 1;
}

static int cur(Lexer *l)  { return (unsigned char)l->src[l->pos]; }
static int cur2(Lexer *l) { return l->src[l->pos] ? (unsigned char)l->src[l->pos + 1] : 0; }

static void adv(Lexer *l) {
    int c = cur(l);
    if (!c) return;
    l->pos++;
    if (c == '\n') { l->line++; l->col = 1; } else l->col++;
}

/* Skips whitespace, // line comments and block comments. */
static void skip_trivia(Lexer *l) {
    for (;;) {
        int c = cur(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            adv(l);
        } else if (c == '/' && cur2(l) == '/') {
            while (cur(l) && cur(l) != '\n') adv(l);
        } else if (c == '/' && cur2(l) == '*') {
            int sl = l->line, sc = l->col;
            adv(l); adv(l);
            for (;;) {
                if (!cur(l)) { diag_error(sl, sc, "unterminated comment"); break; }
                if (cur(l) == '*' && cur2(l) == '/') { adv(l); adv(l); break; }
                adv(l);
            }
        } else break;
    }
}

Token lexer_next(Lexer *l) {
    for (;;) {
        skip_trivia(l);
        Token t;
        memset(&t, 0, sizeof t);
        t.line = l->line; t.col = l->col;
        int c = cur(l);

        if (!c) { t.type = TK_EOF; strcpy(t.text, "<eof>"); return t; }

        if (isalpha(c) || c == '_') {                       /* identifier / keyword */
            int n = 0, over = 0;
            while (isalnum(cur(l)) || cur(l) == '_') {
                if (n < 63) t.text[n++] = (char)cur(l); else over = 1;
                adv(l);
            }
            t.text[n] = 0;
            if (over) diag_error(t.line, t.col, "identifier too long (max 63 characters)");
            t.type = TK_ID;
            for (int k = KW_INT; k <= KW_FALSE; k++)
                if (!strcmp(t.text, NAMES[k])) { t.type = (TokType)k; break; }
            return t;
        }

        if (isdigit(c)) {                                   /* integer literal */
            long long v = 0; int n = 0, over = 0;
            while (isdigit(cur(l))) {
                if (n < 63) t.text[n++] = (char)cur(l);
                v = v * 10 + (cur(l) - '0');
                if (v > INT_MAX) { over = 1; v = INT_MAX; }
                adv(l);
            }
            t.text[n] = 0;
            if (over) diag_error(t.line, t.col, "integer literal '%s' is too large", t.text);
            if (isalpha(cur(l)) || cur(l) == '_') {
                diag_error(l->line, l->col, "invalid character '%c' after number", cur(l));
                while (isalnum(cur(l)) || cur(l) == '_') adv(l);
            }
            t.type = TK_NUM; t.value = (int)v;
            return t;
        }

        adv(l);                                             /* operators / delimiters */
        switch (c) {
        case '+': t.type = TK_PLUS;  break;
        case '-': t.type = TK_MINUS; break;
        case '*': t.type = TK_STAR;  break;
        case '/': t.type = TK_SLASH; break;
        case ';': t.type = TK_SEMI;  break;
        case ',': t.type = TK_COMMA; break;
        case '(': t.type = TK_LPAREN; break;
        case ')': t.type = TK_RPAREN; break;
        case '{': t.type = TK_LBRACE; break;
        case '}': t.type = TK_RBRACE; break;
        case '<': if (cur(l) == '=') { adv(l); t.type = TK_LE; } else t.type = TK_LT; break;
        case '>': if (cur(l) == '=') { adv(l); t.type = TK_GE; } else t.type = TK_GT; break;
        case '=': if (cur(l) == '=') { adv(l); t.type = TK_EQ; } else t.type = TK_ASSIGN; break;
        case '!': if (cur(l) == '=') { adv(l); t.type = TK_NE; } else t.type = TK_NOT; break;
        case '&':
            if (cur(l) == '&') { adv(l); t.type = TK_AND; break; }
            diag_error(t.line, t.col, "unexpected '&' (did you mean '&&'?)");
            continue;
        case '|':
            if (cur(l) == '|') { adv(l); t.type = TK_OR; break; }
            diag_error(t.line, t.col, "unexpected '|' (did you mean '||'?)");
            continue;
        default:
            if (isprint(c)) diag_error(t.line, t.col, "unexpected character '%c'", c);
            else            diag_error(t.line, t.col, "unexpected character (code 0x%02X)", c);
            continue;
        }
        strcpy(t.text, NAMES[t.type]);
        return t;
    }
}
