#include "parser.h"
#include "lexer.h"
#include "csv_writer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *extract_quoted_name(StringView sv)
{
    if (sv.len >= 2)
    {
        char q = sv.data[0];
        if (q == '`' || q == '"')
        {
            char *buf = (char *)malloc(sv.len);
            if (!buf)
                return NULL;
            size_t out = 0;
            for (size_t i = 1; i < sv.len - 1; i++)
            {
                if (sv.data[i] == q && i + 1 < sv.len - 1 && sv.data[i + 1] == q)
                {
                    buf[out++] = q;
                    i++;
                }
                else
                {
                    buf[out++] = sv.data[i];
                }
            }
            buf[out] = '\0';
            return buf;
        }
    }
    return sv_to_cstr(sv);
}

static char *extract_table_name(Token *tok)
{
    if (tok->type == TOK_QUOTED_IDENT)
    {
        return extract_quoted_name(tok->text);
    }
    return sv_to_cstr(tok->text);
}

static void skip_to_semicolon(Lexer *lex)
{
    Token tok;
    for (;;)
    {
        TokenType t = lexer_next(lex, &tok);
        if (t == TOK_SEMICOLON || t == TOK_EOF)
            return;
    }
}

static char *decode_mysql_string(StringView sv, size_t *out_len)
{
    const char *src = sv.data + 1;
    size_t slen = sv.len - 2;
    char *buf = (char *)malloc(slen + 1);
    if (!buf)
    {
        *out_len = 0;
        return NULL;
    }

    size_t out = 0;
    for (size_t i = 0; i < slen; i++)
    {
        char c = src[i];
        if (c == '\\' && i + 1 < slen)
        {
            i++;
            switch (src[i])
            {
            case '0':
                buf[out++] = '\0';
                break;
            case 'n':
                buf[out++] = '\n';
                break;
            case 'r':
                buf[out++] = '\r';
                break;
            case 't':
                buf[out++] = '\t';
                break;
            case 'b':
                buf[out++] = '\b';
                break;
            case '\\':
                buf[out++] = '\\';
                break;
            case '\'':
                buf[out++] = '\'';
                break;
            case '"':
                buf[out++] = '"';
                break;
            case 'Z':
                buf[out++] = '\x1A';
                break; /* SUB (Ctrl+Z) */
            default:
                buf[out++] = src[i];
                break;
            }
        }
        else if (c == '\'' && i + 1 < slen && src[i + 1] == '\'')
        {
            buf[out++] = '\'';
            i++;
        }
        else
        {
            buf[out++] = c;
        }
    }
    buf[out] = '\0';
    *out_len = out;
    return buf;
}

static AppResult parse_create_table_mysql(Lexer *lex, TableRegistry *reg,
                                          ProgressCtx *prog)
{
    Token tok;
    TokenType t;

    t = lexer_next(lex, &tok);

    if (tok.type == TOK_KW_IF)
    {
        lexer_next(lex, &tok);
        lexer_next(lex, &tok);
        t = lexer_next(lex, &tok);
    }

    if (t != TOK_QUOTED_IDENT && t != TOK_IDENT)
    {
        skip_to_semicolon(lex);
        return OK;
    }

    char *table_name = extract_table_name(&tok);
    if (!table_name)
        return ERR_MEMORY;

    t = lexer_next(lex, &tok);
    if (t != TOK_LPAREN)
    {
        free(table_name);
        skip_to_semicolon(lex);
        return OK;
    }

    char *columns[4096];
    int col_count = 0;
    int depth = 1;

    while (depth > 0)
    {
        t = lexer_next(lex, &tok);
        if (t == TOK_EOF)
            break;
        if (t == TOK_RPAREN)
        {
            depth--;
            break;
        }

        if (t == TOK_KW_PRIMARY || t == TOK_KW_KEY || t == TOK_KW_UNIQUE ||
            t == TOK_KW_INDEX || t == TOK_KW_CONSTRAINT || t == TOK_KW_CHECK ||
            t == TOK_KW_FOREIGN)
        {
            int inner_depth = 0;
            for (;;)
            {
                t = lexer_next(lex, &tok);
                if (t == TOK_EOF)
                    goto done;
                if (t == TOK_LPAREN)
                    inner_depth++;
                else if (t == TOK_RPAREN)
                {
                    if (inner_depth > 0)
                        inner_depth--;
                    else
                    {
                        depth--;
                        goto done;
                    }
                }
                else if (t == TOK_COMMA && inner_depth == 0)
                    break;
            }
            continue;
        }

        if ((t == TOK_QUOTED_IDENT || t == TOK_IDENT) && col_count < 4096)
        {
            char *col_name = extract_table_name(&tok);
            if (col_name)
            {
                columns[col_count++] = col_name;
            }

            int inner_depth = 0;
            for (;;)
            {
                t = lexer_next(lex, &tok);
                if (t == TOK_EOF)
                    goto done;
                if (t == TOK_LPAREN)
                    inner_depth++;
                else if (t == TOK_RPAREN)
                {
                    if (inner_depth > 0)
                        inner_depth--;
                    else
                    {
                        depth--;
                        goto done;
                    }
                }
                else if (t == TOK_COMMA && inner_depth == 0)
                    break;
            }
        }
    }

done:
    if (col_count > 0)
    {
        AppResult res = registry_add_table(reg, table_name, columns, col_count);
        if (res != OK)
        {
            for (int i = 0; i < col_count; i++)
                free(columns[i]);
            free(table_name);
            return res;
        }
        progress_new_table(prog, table_name, col_count);
    }

    for (int i = 0; i < col_count; i++)
        free(columns[i]);
    free(table_name);

    skip_to_semicolon(lex);
    return OK;
}

static AppResult parse_insert_mysql(Lexer *lex, TableRegistry *reg,
                                    ProgressCtx *prog)
{
    Token tok;
    TokenType t;

    t = lexer_next(lex, &tok);
    if (t != TOK_KW_INTO)
    {
        skip_to_semicolon(lex);
        return OK;
    }

    t = lexer_next(lex, &tok);
    if (t != TOK_QUOTED_IDENT && t != TOK_IDENT)
    {
        skip_to_semicolon(lex);
        return OK;
    }

    char *table_name = extract_table_name(&tok);
    if (!table_name)
        return ERR_MEMORY;

    t = lexer_next(lex, &tok);

    if (t == TOK_DOT)
    {
        free(table_name);
        t = lexer_next(lex, &tok);
        if (t != TOK_QUOTED_IDENT && t != TOK_IDENT)
        {
            skip_to_semicolon(lex);
            return OK;
        }
        table_name = extract_table_name(&tok);
        if (!table_name)
            return ERR_MEMORY;
        t = lexer_next(lex, &tok);
    }

    char **insert_columns = NULL;
    int insert_col_count = 0;

    if (t == TOK_LPAREN)
    {
        char *cols[4096];
        int ncols = 0;
        int is_column_list = 0;

        Token peek;
        TokenType pt = lexer_next(lex, &peek);
        if (pt == TOK_QUOTED_IDENT || pt == TOK_IDENT)
        {
            is_column_list = 1;
            cols[ncols++] = extract_table_name(&peek);
            for (;;)
            {
                pt = lexer_next(lex, &peek);
                if (pt == TOK_RPAREN)
                    break;
                if (pt == TOK_COMMA)
                {
                    pt = lexer_next(lex, &peek);
                    if ((pt == TOK_QUOTED_IDENT || pt == TOK_IDENT) && ncols < 4096)
                    {
                        cols[ncols++] = extract_table_name(&peek);
                    }
                }
                if (pt == TOK_EOF)
                    break;
            }
            insert_columns = cols;
            insert_col_count = ncols;

            t = lexer_next(lex, &tok);
        }
        else
        {
            is_column_list = 0;
        }

        if (!is_column_list)
        {
            goto parse_first_value_tuple;
        }
    }

    TableInfo *table = registry_find(reg, table_name);

    if (!table && insert_columns && insert_col_count > 0)
    {
        AppResult rr = registry_add_table(reg, table_name, insert_columns,
                                          insert_col_count);
        if (rr != OK)
        {
            for (int i = 0; i < insert_col_count; i++)
                free(insert_columns[i]);
            free(table_name);
            return rr;
        }
        table = registry_find(reg, table_name);
        progress_new_table(prog, table_name, insert_col_count);
    }

    if (!table)
    {
        free(table_name);
        skip_to_semicolon(lex);
        return OK;
    }

    if (t != TOK_KW_VALUES && t != TOK_KW_VALUE)
    {
        if (insert_columns)
        {
            for (int i = 0; i < insert_col_count; i++)
                free(insert_columns[i]);
        }
        free(table_name);
        skip_to_semicolon(lex);
        return OK;
    }

    for (;;)
    {
        t = lexer_next(lex, &tok);
        if (t == TOK_SEMICOLON || t == TOK_EOF)
            break;
        if (t == TOK_COMMA)
            continue;

        if (t != TOK_LPAREN)
            continue;

        {
            int field_idx = 0;
            int col_limit = table->col_count;

            for (;;)
            {
                t = lexer_next(lex, &tok);
                if (t == TOK_RPAREN || t == TOK_EOF)
                    break;
                if (t == TOK_COMMA)
                    continue;

                int is_last = (field_idx == col_limit - 1);

                if (t == TOK_KW_NULL)
                {
                    csv_write_null(table->csv_fp, is_last);
                }
                else if (t == TOK_STRING)
                {
                    size_t decoded_len;
                    char *decoded = decode_mysql_string(tok.text, &decoded_len);
                    if (decoded)
                    {
                        csv_write_field(table->csv_fp, decoded, decoded_len, is_last);
                        free(decoded);
                    }
                    else
                    {
                        csv_write_null(table->csv_fp, is_last);
                    }
                }
                else if (t == TOK_NUMBER || t == TOK_HEX || t == TOK_IDENT ||
                         t == TOK_KW_TRUE || t == TOK_KW_FALSE || t == TOK_KW_DEFAULT)
                {
                    csv_write_field_sv(table->csv_fp, tok.text, is_last);
                }
                else
                {
                    csv_write_field_sv(table->csv_fp, tok.text, is_last);
                }

                field_idx++;
                if (is_last)
                {
                }
            }

            while (field_idx < col_limit)
            {
                int is_last = (field_idx == col_limit - 1);
                csv_write_null(table->csv_fp, is_last);
                field_idx++;
            }

            table->row_count++;
            reg->total_rows++;
            prog->total_rows++;
        }
    }

    if (insert_columns)
    {
        for (int i = 0; i < insert_col_count; i++)
            free(insert_columns[i]);
    }
    free(table_name);
    return OK;

parse_first_value_tuple:
    free(table_name);
    skip_to_semicolon(lex);
    return OK;
}

AppResult parse_mysql(StreamReader *sr, TableRegistry *reg, ProgressCtx *prog)
{
    Lexer lex;
    lexer_init(&lex, sr);

    Token tok;
    AppResult res = OK;

    for (;;)
    {
        TokenType t = lexer_next(&lex, &tok);
        if (t == TOK_EOF)
            break;

        progress_update(prog, sr->total_read - (int64_t)sr_available(sr));

        if (t == TOK_KW_CREATE)
        {
            Token next;
            TokenType nt = lexer_next(&lex, &next);
            if (nt == TOK_KW_TABLE)
            {
                res = parse_create_table_mysql(&lex, reg, prog);
                if (res != OK)
                    return res;
            }
            else
            {
                skip_to_semicolon(&lex);
            }
        }
        else if (t == TOK_KW_INSERT)
        {
            res = parse_insert_mysql(&lex, reg, prog);
            if (res != OK)
                return res;
        }
        else if (t == TOK_SEMICOLON)
        {
            continue;
        }
        else
        {
            skip_to_semicolon(&lex);
        }
    }

    return OK;
}
