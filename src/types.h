#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define TOOL_VERSION "1.0.0"

#define BUF_MB_DEFAULT 4
#define BUF_SIZE_DEFAULT (BUF_MB_DEFAULT * 1024 * 1024)

#define CSV_BUF_SIZE (256 * 1024)

typedef struct
{
    const char *data;
    size_t len;
} StringView;

static inline StringView sv_from_cstr(const char *s)
{
    StringView sv;
    sv.data = s;
    sv.len = s ? strlen(s) : 0;
    return sv;
}

static inline bool sv_equals_cstr(StringView sv, const char *s)
{
    size_t slen = strlen(s);
    if (sv.len != slen)
        return false;
    return memcmp(sv.data, s, slen) == 0;
}

static inline bool sv_iequals_cstr(StringView sv, const char *s)
{
    size_t slen = strlen(s);
    if (sv.len != slen)
        return false;
    for (size_t i = 0; i < slen; i++)
    {
        char a = sv.data[i];
        char b = s[i];
        if (a >= 'A' && a <= 'Z')
            a += 32;
        if (b >= 'A' && b <= 'Z')
            b += 32;
        if (a != b)
            return false;
    }
    return true;
}

typedef enum
{
    FORMAT_UNKNOWN = 0,
    FORMAT_MYSQL,
    FORMAT_PGSQL
} SqlFormat;

typedef enum
{
    OK = 0,
    ERR_IO,
    ERR_MEMORY,
    ERR_PARSE,
    ERR_ARGS,
    ERR_FORMAT
} AppResult;

#endif /* TYPES_H */
