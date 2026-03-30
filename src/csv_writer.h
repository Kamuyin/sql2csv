#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include "types.h"
#include <stdio.h>

void csv_set_encoding(OutputEncoding enc);

FILE *csv_open(const char *path);

void csv_write_header(FILE *fp, char **columns, int col_count);

void csv_write_field(FILE *fp, const char *data, size_t len, int is_last);

static inline void csv_write_field_sv(FILE *fp, StringView sv, int is_last)
{
    csv_write_field(fp, sv.data, sv.len, is_last);
}

void csv_end_row(FILE *fp);

static inline void csv_write_null(FILE *fp, int is_last)
{
    csv_write_field(fp, NULL, 0, is_last);
}

#endif /* CSV_WRITER_H */
