#ifndef STREAM_READER_H
#define STREAM_READER_H

#include "types.h"
#include <stdio.h>

typedef struct
{
    FILE *fp;
    char *buf;
    size_t buf_size;
    size_t pos;
    size_t valid;
    int eof_reached;
    int64_t total_read;
    int64_t file_size;
    size_t mark;
} StreamReader;

AppResult sr_open(StreamReader *sr, const char *path, int buf_mb);

void sr_close(StreamReader *sr);

size_t sr_ensure(StreamReader *sr, size_t n);

static inline int sr_peek_char(StreamReader *sr)
{
    if (sr->pos >= sr->valid)
    {
        if (sr_ensure(sr, 1) == 0)
            return -1;
    }
    return (unsigned char)sr->buf[sr->pos];
}

static inline int sr_peek_at(StreamReader *sr, size_t off)
{
    if (sr->pos + off >= sr->valid)
    {
        if (sr_ensure(sr, off + 1) <= off)
            return -1;
    }
    return (unsigned char)sr->buf[sr->pos + off];
}

static inline const char *sr_ptr(StreamReader *sr)
{
    return sr->buf + sr->pos;
}

static inline size_t sr_available(StreamReader *sr)
{
    return sr->valid - sr->pos;
}

static inline void sr_advance(StreamReader *sr, size_t n)
{
    sr->pos += n;
    if (sr->pos > sr->valid)
        sr->pos = sr->valid;
}

static inline int sr_eof(StreamReader *sr)
{
    return sr->eof_reached && sr->pos >= sr->valid;
}

static inline const char *sr_peek(StreamReader *sr, size_t n, size_t *out_len)
{
    size_t avail = sr_ensure(sr, n);
    if (out_len)
        *out_len = avail;
    return sr->buf + sr->pos;
}

#endif /* STREAM_READER_H */
