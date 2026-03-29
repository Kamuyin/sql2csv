#include "stream_reader.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <sys/stat.h>
#define stat_t struct _stat64
#define fstat_fn _fstat64
#define fileno_fn _fileno
#else
#include <sys/stat.h>
#include <unistd.h>
#define stat_t struct stat
#define fstat_fn fstat
#define fileno_fn fileno
#endif

static int64_t get_file_size(FILE *fp)
{
    stat_t st;
    if (fstat_fn(fileno_fn(fp), &st) == 0)
    {
        return (int64_t)st.st_size;
    }
    return -1;
}

AppResult sr_open(StreamReader *sr, const char *path, int buf_mb)
{
    memset(sr, 0, sizeof(*sr));

    sr->fp = fopen(path, "rb");
    if (!sr->fp)
        return ERR_IO;

    sr->buf_size = (buf_mb > 0)
                       ? ((size_t)buf_mb * 1024 * 1024)
                       : BUF_SIZE_DEFAULT;

    sr->buf = (char *)malloc(sr->buf_size);
    if (!sr->buf)
    {
        fclose(sr->fp);
        sr->fp = NULL;
        return ERR_MEMORY;
    }

    sr->file_size = get_file_size(sr->fp);

    sr->valid = fread(sr->buf, 1, sr->buf_size, sr->fp);
    sr->total_read = (int64_t)sr->valid;
    if (sr->valid < sr->buf_size)
    {
        sr->eof_reached = 1;
    }

    sr->pos = 0;
    sr->mark = (size_t)-1;
    return OK;
}

void sr_close(StreamReader *sr)
{
    if (sr->fp)
    {
        fclose(sr->fp);
        sr->fp = NULL;
    }
    free(sr->buf);
    sr->buf = NULL;
    sr->valid = 0;
    sr->pos = 0;
}

size_t sr_ensure(StreamReader *sr, size_t n)
{
    size_t avail = sr->valid - sr->pos;
    if (avail >= n)
        return avail;
    if (sr->eof_reached)
        return avail;

    size_t origin = sr->pos;
    if (sr->mark != (size_t)-1 && sr->mark < sr->pos)
    {
        origin = sr->mark;
    }
    size_t keep = sr->valid - origin;
    if (origin > 0 && keep < sr->buf_size)
    {
        memmove(sr->buf, sr->buf + origin, keep);
        sr->pos -= origin;
        sr->valid = keep;
        if (sr->mark != (size_t)-1)
            sr->mark -= origin;
    }

    size_t space = sr->buf_size - sr->valid;
    if (space > 0)
    {
        size_t got = fread(sr->buf + sr->valid, 1, space, sr->fp);
        sr->valid += got;
        sr->total_read += (int64_t)got;
        if (got < space)
        {
            sr->eof_reached = 1;
        }
    }

    return sr->valid - sr->pos;
}
