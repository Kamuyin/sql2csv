#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *sv_to_cstr(StringView sv) {
    char *s = (char *)malloc(sv.len + 1);
    if (!s) return NULL;
    memcpy(s, sv.data, sv.len);
    s[sv.len] = '\0';
    return s;
}

void lexer_init(Lexer *lex, StreamReader *sr) {
    lex->sr = sr;
    lex->raw_mode = 0;
}

typedef struct { const char *word; TokenType type; } KeywordEntry;

static const KeywordEntry keywords[] = {
    {"CREATE",     TOK_KW_CREATE},
    {"TABLE",      TOK_KW_TABLE},
    {"INSERT",     TOK_KW_INSERT},
    {"INTO",       TOK_KW_INTO},
    {"VALUES",     TOK_KW_VALUES},
    {"VALUE",      TOK_KW_VALUE},
    {"COPY",       TOK_KW_COPY},
    {"FROM",       TOK_KW_FROM},
    {"STDIN",      TOK_KW_STDIN},
    {"NULL",       TOK_KW_NULL},
    {"TRUE",       TOK_KW_TRUE},
    {"FALSE",      TOK_KW_FALSE},
    {"DEFAULT",    TOK_KW_DEFAULT},
    {"IF",         TOK_KW_IF},
    {"NOT",        TOK_KW_NOT},
    {"EXISTS",     TOK_KW_EXISTS},
    {"DROP",       TOK_KW_DROP},
    {"LOCK",       TOK_KW_LOCK},
    {"UNLOCK",     TOK_KW_UNLOCK},
    {"SET",        TOK_KW_SET},
    {"ALTER",      TOK_KW_ALTER},
    {"PRIMARY",    TOK_KW_PRIMARY},
    {"KEY",        TOK_KW_KEY},
    {"UNIQUE",     TOK_KW_UNIQUE},
    {"INDEX",      TOK_KW_INDEX},
    {"CONSTRAINT", TOK_KW_CONSTRAINT},
    {"FOREIGN",    TOK_KW_FOREIGN},
    {"CHECK",      TOK_KW_CHECK},
    {"REFERENCES", TOK_KW_REFERENCES},
    {"USING",      TOK_KW_USING},
    {NULL, TOK_EOF}
};

static TokenType classify_ident(StringView sv) {
    for (const KeywordEntry *kw = keywords; kw->word; kw++) {
        if (sv_iequals_cstr(sv, kw->word)) return kw->type;
    }
    return TOK_IDENT;
}

static void skip_whitespace(Lexer *lex) {
    StreamReader *sr = lex->sr;
    for (;;) {
        if (sr_ensure(sr, 1) == 0) return;
        char c = sr->buf[sr->pos];
        if (lex->raw_mode && (c == '\n' || c == '\r')) return;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            sr_advance(sr, 1);
        } else {
            return;
        }
    }
}

static void skip_line_comment(Lexer *lex) {
    StreamReader *sr = lex->sr;
    for (;;) {
        if (sr_ensure(sr, 1) == 0) return;
        char c = sr->buf[sr->pos];
        sr_advance(sr, 1);
        if (c == '\n') return;
    }
}

static int skip_block_comment(Lexer *lex) {
    StreamReader *sr = lex->sr;

    for (;;) {
        if (sr_ensure(sr, 2) == 0) return 0;
        if (sr->buf[sr->pos] == '*' && sr->buf[sr->pos + 1] == '/') {
            sr_advance(sr, 2);
            return 0;
        }
        sr_advance(sr, 1);
    }
}

static void skip_ws_and_comments(Lexer *lex) {
    StreamReader *sr = lex->sr;
    for (;;) {
        skip_whitespace(lex);
        if (sr_ensure(sr, 2) == 0) return;

        char c0 = sr->buf[sr->pos];
        char c1 = sr->buf[sr->pos + 1];

        if (c0 == '-' && c1 == '-') {
            sr_advance(sr, 2);
            skip_line_comment(lex);
            continue;
        }

        if (c0 == '#') {
            sr_advance(sr, 1);
            skip_line_comment(lex);
            continue;
        }

        if (c0 == '/' && c1 == '*') {
            sr_advance(sr, 2);
            if (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == '!') {
                /* MySQL conditional comment: treat content as live SQL, not a comment */
                sr_advance(sr, 1);
                while (sr_ensure(sr, 1) > 0 && isdigit((unsigned char)sr->buf[sr->pos])) {
                    sr_advance(sr, 1);
                }
                while (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == ' ') {
                    sr_advance(sr, 1);
                }

                return;
            }
            skip_block_comment(lex);
            continue;
        }

        if (c0 == '*' && c1 == '/') {
            /* closing end-marker of a conditional comment */
            sr_advance(sr, 2);
            continue;
        }

        return;
    }
}

static TokenType lex_string(Lexer *lex, Token *tok) {
    StreamReader *sr = lex->sr;
    sr->mark = sr->pos;
    sr_advance(sr, 1);

    for (;;) {
        if (sr_ensure(sr, 1) == 0) {
            tok->type = TOK_ERROR;
            tok->text.data = sr->buf + sr->mark;
            tok->text.len = sr->pos - sr->mark;
            sr->mark = (size_t)-1;
            return TOK_ERROR;
        }
        char c = sr->buf[sr->pos];
        if (c == '\\') {
            sr_advance(sr, 1);
            if (sr_ensure(sr, 1) > 0) sr_advance(sr, 1);
            continue;
        }
        if (c == '\'') {
            sr_advance(sr, 1);
            if (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == '\'') {
                sr_advance(sr, 1);
                continue;
            }
            break;
        }
        sr_advance(sr, 1);
    }

    tok->type = TOK_STRING;
    tok->text.data = sr->buf + sr->mark;
    tok->text.len = sr->pos - sr->mark;
    sr->mark = (size_t)-1;
    return TOK_STRING;
}

static TokenType lex_quoted_ident(Lexer *lex, Token *tok, char quote) {
    StreamReader *sr = lex->sr;
    sr->mark = sr->pos;
    sr_advance(sr, 1);

    for (;;) {
        if (sr_ensure(sr, 1) == 0) {
            tok->type = TOK_ERROR;
            tok->text.data = sr->buf + sr->mark;
            tok->text.len = sr->pos - sr->mark;
            sr->mark = (size_t)-1;
            return TOK_ERROR;
        }
        char c = sr->buf[sr->pos];
        if (c == quote) {
            sr_advance(sr, 1);
            if (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == quote) {
                sr_advance(sr, 1);
                continue;
            }
            break;
        }
        sr_advance(sr, 1);
    }

    tok->type = TOK_QUOTED_IDENT;
    tok->text.data = sr->buf + sr->mark;
    tok->text.len = sr->pos - sr->mark;
    sr->mark = (size_t)-1;
    return TOK_QUOTED_IDENT;
}

static TokenType lex_number(Lexer *lex, Token *tok) {
    StreamReader *sr = lex->sr;
    sr->mark = sr->pos;

    if (sr_ensure(sr, 1) > 0 && (sr->buf[sr->pos] == '-' || sr->buf[sr->pos] == '+')) {
        sr_advance(sr, 1);
    }

    while (sr_ensure(sr, 1) > 0 && isdigit((unsigned char)sr->buf[sr->pos])) {
        sr_advance(sr, 1);
    }

    if (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == '.') {
        sr_advance(sr, 1);
        while (sr_ensure(sr, 1) > 0 && isdigit((unsigned char)sr->buf[sr->pos])) {
            sr_advance(sr, 1);
        }
    }

    if (sr_ensure(sr, 1) > 0 && (sr->buf[sr->pos] == 'e' || sr->buf[sr->pos] == 'E')) {
        sr_advance(sr, 1);
        if (sr_ensure(sr, 1) > 0 && (sr->buf[sr->pos] == '-' || sr->buf[sr->pos] == '+')) {
            sr_advance(sr, 1);
        }
        while (sr_ensure(sr, 1) > 0 && isdigit((unsigned char)sr->buf[sr->pos])) {
            sr_advance(sr, 1);
        }
    }

    tok->type = TOK_NUMBER;
    tok->text.data = sr->buf + sr->mark;
    tok->text.len = sr->pos - sr->mark;
    sr->mark = (size_t)-1;
    return TOK_NUMBER;
}

static TokenType lex_hex(Lexer *lex, Token *tok, int x_prefix) {
    StreamReader *sr = lex->sr;
    sr->mark = sr->pos;

    if (x_prefix) {
        sr_advance(sr, 2);
        while (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] != '\'') {
            sr_advance(sr, 1);
        }
        if (sr_ensure(sr, 1) > 0) sr_advance(sr, 1);
    } else {
        sr_advance(sr, 2);
        while (sr_ensure(sr, 1) > 0 && isxdigit((unsigned char)sr->buf[sr->pos])) {
            sr_advance(sr, 1);
        }
    }

    tok->type = TOK_HEX;
    tok->text.data = sr->buf + sr->mark;
    tok->text.len = sr->pos - sr->mark;
    sr->mark = (size_t)-1;
    return TOK_HEX;
}

static TokenType lex_ident(Lexer *lex, Token *tok) {
    StreamReader *sr = lex->sr;
    sr->mark = sr->pos;

    while (sr_ensure(sr, 1) > 0) {
        char c = sr->buf[sr->pos];
        if (isalnum((unsigned char)c) || c == '_' || c == '$') {
            sr_advance(sr, 1);
        } else {
            break;
        }
    }

    tok->text.data = sr->buf + sr->mark;
    tok->text.len = sr->pos - sr->mark;
    tok->type = classify_ident(tok->text);
    sr->mark = (size_t)-1;
    return tok->type;
}

TokenType lexer_next(Lexer *lex, Token *tok) {
    StreamReader *sr = lex->sr;

    tok->type = TOK_EOF;
    tok->text.data = NULL;
    tok->text.len = 0;

    if (lex->raw_mode) {
        while (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == '\r') {
            sr_advance(sr, 1);
        }
        if (sr_eof(sr)) return TOK_EOF;

        if (sr->buf[sr->pos] == '\n') {
            tok->type = TOK_NEWLINE;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_NEWLINE;
        }

        if (sr->buf[sr->pos] == '\\' && sr_ensure(sr, 2) >= 2 && sr->buf[sr->pos + 1] == '.') {
            tok->type = TOK_COPY_END;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 2;
            sr_advance(sr, 2);
            if (sr_ensure(sr, 1) > 0 && (sr->buf[sr->pos] == '\n' || sr->buf[sr->pos] == '\r')) {
                if (sr->buf[sr->pos] == '\r') sr_advance(sr, 1);
                if (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] == '\n') sr_advance(sr, 1);
            }
            return TOK_COPY_END;
        }

        {
            sr->mark = sr->pos;
            while (sr_ensure(sr, 1) > 0 && sr->buf[sr->pos] != '\n' && sr->buf[sr->pos] != '\r') {
                sr_advance(sr, 1);
            }
            tok->type = TOK_STRING;
            tok->text.data = sr->buf + sr->mark;
            tok->text.len = sr->pos - sr->mark;
            sr->mark = (size_t)-1;
            return TOK_STRING;
        }
    }

    skip_ws_and_comments(lex);
    if (sr_ensure(sr, 1) == 0) return TOK_EOF;

    char c = sr->buf[sr->pos];

    switch (c) {
        case '(':
            tok->type = TOK_LPAREN;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_LPAREN;
        case ')':
            tok->type = TOK_RPAREN;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_RPAREN;
        case ',':
            tok->type = TOK_COMMA;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_COMMA;
        case ';':
            tok->type = TOK_SEMICOLON;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_SEMICOLON;
        case '.':
            tok->type = TOK_DOT;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_DOT;
        case '=':
            tok->type = TOK_EQUALS;
            tok->text.data = sr->buf + sr->pos;
            tok->text.len = 1;
            sr_advance(sr, 1);
            return TOK_EQUALS;
        default:
            break;
    }

    if (c == '\'') {
        return lex_string(lex, tok);
    }
    if (c == '`') {
        return lex_quoted_ident(lex, tok, '`');
    }
    if (c == '"') {
        return lex_quoted_ident(lex, tok, '"');
    }
    if (c == '0' && sr_ensure(sr, 2) >= 2 &&
        (sr->buf[sr->pos + 1] == 'x' || sr->buf[sr->pos + 1] == 'X')) {
        return lex_hex(lex, tok, 0);
    }
    if ((c == 'X' || c == 'x') && sr_ensure(sr, 2) >= 2 && sr->buf[sr->pos + 1] == '\'') {
        return lex_hex(lex, tok, 1);
    }
    if ((c == 'E' || c == 'e') && sr_ensure(sr, 2) >= 2 && sr->buf[sr->pos + 1] == '\'') {
        sr_advance(sr, 1);
        return lex_string(lex, tok);
    }
    if ((c == 'B' || c == 'b') && sr_ensure(sr, 2) >= 2 && sr->buf[sr->pos + 1] == '\'') {
        return lex_hex(lex, tok, 1);
    }
    if (isdigit((unsigned char)c)) {
        return lex_number(lex, tok);
    }
    if (c == '-' && sr_ensure(sr, 2) >= 2 && isdigit((unsigned char)sr->buf[sr->pos + 1])) {
        return lex_number(lex, tok);
    }
    if (isalpha((unsigned char)c) || c == '_') {
        return lex_ident(lex, tok);
    }
    tok->type = TOK_ERROR;
    tok->text.data = sr->buf + sr->pos;
    tok->text.len = 1;
    sr_advance(sr, 1);
    return TOK_ERROR;
}
