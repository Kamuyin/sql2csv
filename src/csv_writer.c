#include "csv_writer.h"
#include <stdlib.h>
#include <string.h>

static OutputEncoding g_encoding = ENCODING_PASSTHROUGH;

void csv_set_encoding(OutputEncoding enc)
{
    g_encoding = enc;
}

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

static void write_encoded(FILE *fp, const char *data, size_t len)
{
    if (len == 0)
        return;

    if (g_encoding == ENCODING_LATIN1)
    {
        for (size_t i = 0; i < len; i++)
        {
            unsigned char b = (unsigned char)data[i];
            if (b < 0x80)
            {
                fputc(b, fp);
            }
            else
            {
                /* 2-byte UTF-8: 110xxxxx 10xxxxxx */
                fputc((int)(0xC0 | (b >> 6)), fp);
                fputc((int)(0x80 | (b & 0x3F)), fp);
            }
        }
        return;
    }

    if (g_encoding == ENCODING_SANITIZE)
    {
        size_t i = 0;
        while (i < len)
        {
            unsigned char b = (unsigned char)data[i];
            int seq;
            if (b < 0x80)
            {
                fputc(b, fp);
                i++;
                continue;
            }
            else if ((b & 0xE0) == 0xC0 && b >= 0xC2)
                seq = 2;
            else if ((b & 0xF0) == 0xE0)
                seq = 3;
            else if ((b & 0xF8) == 0xF0 && b <= 0xF4)
                seq = 4;
            else
            {
                fputc('?', fp);
                i++;
                continue;
            }

            if (i + (size_t)seq > len)
            {
                fputc('?', fp);
                i++;
                continue;
            }

            int valid = 1;
            for (int j = 1; j < seq; j++)
            {
                if (((unsigned char)data[i + j] & 0xC0) != 0x80)
                {
                    valid = 0;
                    break;
                }
            }
            if (valid)
            {
                fwrite(data + i, 1, (size_t)seq, fp);
                i += (size_t)seq;
            }
            else
            {
                fputc('?', fp);
                i++;
            }
        }
        return;
    }

    /* ENCODING_PASSTHROUGH */
    fwrite(data, 1, len, fp);
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
            size_t seg = 0;
            for (size_t j = 0; j < len; j++)
            {
                if (col[j] == '"')
                {
                    write_encoded(fp, col + seg, j - seg);
                    fputc('"', fp);
                    fputc('"', fp);
                    seg = j + 1;
                }
            }
            write_encoded(fp, col + seg, len - seg);
            fputc('"', fp);
        }
        else
        {
            write_encoded(fp, col, len);
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
    if (data != NULL && len > 0)
    {
        if (needs_quoting(data, len))
        {
            fputc('"', fp);
            size_t seg = 0;
            for (size_t i = 0; i < len; i++)
            {
                if (data[i] == '"')
                {
                    write_encoded(fp, data + seg, i - seg);
                    fputc('"', fp);
                    fputc('"', fp);
                    seg = i + 1;
                }
            }
            write_encoded(fp, data + seg, len - seg);
            fputc('"', fp);
        }
        else
        {
            write_encoded(fp, data, len);
        }
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
