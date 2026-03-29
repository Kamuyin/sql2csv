#include "csv_writer.h"
#include <stdlib.h>
#include <string.h>

FILE *csv_open(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return NULL;

    char *buf = (char *)malloc(CSV_BUF_SIZE);
    if (buf)
    {
        setvbuf(fp, buf, _IOFBF, CSV_BUF_SIZE);
    }
    return fp;
}

static int needs_quoting(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char c = data[i];
        if (c == ',' || c == '"' || c == '\r' || c == '\n')
            return 1;
    }
    return 0;
}

void csv_write_header(FILE *fp, char **columns, int col_count)
{
    for (int i = 0; i < col_count; i++)
    {
        const char *col = columns[i];
        size_t len = strlen(col);
        if (needs_quoting(col, len))
        {
            fputc('"', fp);
            for (size_t j = 0; j < len; j++)
            {
                if (col[j] == '"')
                    fputc('"', fp);
                fputc(col[j], fp);
            }
            fputc('"', fp);
        }
        else
        {
            fwrite(col, 1, len, fp);
        }
        if (i < col_count - 1)
        {
            fputc(',', fp);
        }
    }
    fputc('\r', fp);
    fputc('\n', fp);
}

void csv_write_field(FILE *fp, const char *data, size_t len, int is_last)
{
    if (data == NULL || len == 0)
    {
        /* empty field */
    }
    else if (needs_quoting(data, len))
    {
        fputc('"', fp);
        for (size_t i = 0; i < len; i++)
        {
            if (data[i] == '"')
                fputc('"', fp);
            fputc(data[i], fp);
        }
        fputc('"', fp);
    }
    else
    {
        fwrite(data, 1, len, fp);
    }

    if (is_last)
    {
        fputc('\r', fp);
        fputc('\n', fp);
    }
    else
    {
        fputc(',', fp);
    }
}

void csv_end_row(FILE *fp)
{
    fputc('\r', fp);
    fputc('\n', fp);
}
