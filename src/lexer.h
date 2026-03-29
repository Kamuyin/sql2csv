#ifndef LEXER_H
#define LEXER_H

#include "types.h"
#include "stream_reader.h"

typedef enum {
    TOK_EOF = 0,
    TOK_ERROR,

    TOK_IDENT,
    TOK_QUOTED_IDENT,
    TOK_STRING,
    TOK_NUMBER,
    TOK_HEX,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_SEMICOLON,
    TOK_DOT,
    TOK_EQUALS,

    TOK_NEWLINE,
    TOK_COPY_END,

    /* Keywords */
    TOK_KW_CREATE,
    TOK_KW_TABLE,
    TOK_KW_INSERT,
    TOK_KW_INTO,
    TOK_KW_VALUES,
    TOK_KW_VALUE,
    TOK_KW_COPY,
    TOK_KW_FROM,
    TOK_KW_STDIN,
    TOK_KW_NULL,
    TOK_KW_TRUE,
    TOK_KW_FALSE,
    TOK_KW_DEFAULT,
    TOK_KW_IF,
    TOK_KW_NOT,
    TOK_KW_EXISTS,
    TOK_KW_DROP,
    TOK_KW_LOCK,
    TOK_KW_UNLOCK,
    TOK_KW_SET,
    TOK_KW_ALTER,
    TOK_KW_PRIMARY,
    TOK_KW_KEY,
    TOK_KW_UNIQUE,
    TOK_KW_INDEX,
    TOK_KW_CONSTRAINT,
    TOK_KW_FOREIGN,
    TOK_KW_CHECK,
    TOK_KW_REFERENCES,
    TOK_KW_USING
} TokenType;

typedef struct {
    TokenType  type;
    StringView text;
} Token;

typedef struct {
    StreamReader *sr;
    int           raw_mode;
} Lexer;

void lexer_init(Lexer *lex, StreamReader *sr);
TokenType lexer_next(Lexer *lex, Token *tok);

static inline void lexer_set_raw_mode(Lexer *lex, int on) {
    lex->raw_mode = on;
}

static inline int tok_is_keyword(const Token *tok, TokenType kw) {
    return tok->type == kw;
}

char *sv_to_cstr(StringView sv);

#endif /* LEXER_H */
