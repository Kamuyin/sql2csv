#ifndef PROGRESS_H
#define PROGRESS_H

#include "types.h"
#include <stdio.h>
#include <time.h>

typedef struct
{
    int64_t file_size;
    int table_count;
    int64_t total_rows;
    int verbose;
    clock_t last_report;
    clock_t start_time;
} ProgressCtx;

static inline void progress_init(ProgressCtx *ctx, int64_t file_size, int verbose)
{
    ctx->file_size = file_size;
    ctx->table_count = 0;
    ctx->total_rows = 0;
    ctx->verbose = verbose;
    ctx->last_report = clock();
    ctx->start_time = clock();
}

static inline void progress_set_format(const char *format_name)
{
    fprintf(stderr, "[sql2csv] Auto-detected format: %s\n", format_name);
}

static inline void progress_new_table(ProgressCtx *ctx, const char *name, int cols)
{
    ctx->table_count++;
    if (ctx->verbose)
    {
        fprintf(stderr, "[sql2csv] Processing: %s (%d columns)\n", name, cols);
    }
}

static inline void progress_update(ProgressCtx *ctx, int64_t bytes_read)
{
    clock_t now = clock();
    double elapsed_since_last = (double)(now - ctx->last_report) / CLOCKS_PER_SEC;

    if (elapsed_since_last < 1.0)
        return;
    ctx->last_report = now;

    if (ctx->file_size > 0)
    {
        double pct = (double)bytes_read / (double)ctx->file_size * 100.0;
        double mb_read = (double)bytes_read / (1024.0 * 1024.0);
        double mb_total = (double)ctx->file_size / (1024.0 * 1024.0);
        fprintf(stderr, "\r[sql2csv]   Read: %.1f MB / %.1f MB (%.1f%%) | Tables: %d | Rows: %lld",
                mb_read, mb_total, pct, ctx->table_count, (long long)ctx->total_rows);
    }
    else
    {
        double mb_read = (double)bytes_read / (1024.0 * 1024.0);
        fprintf(stderr, "\r[sql2csv]   Read: %.1f MB | Tables: %d | Rows: %lld",
                mb_read, ctx->table_count, (long long)ctx->total_rows);
    }
}

static inline void progress_finish(ProgressCtx *ctx, int64_t bytes_read)
{
    clock_t now = clock();
    double elapsed = (double)(now - ctx->start_time) / CLOCKS_PER_SEC;
    double mb = (double)bytes_read / (1024.0 * 1024.0);

    int mins = (int)(elapsed / 60.0);
    int secs = (int)(elapsed) % 60;

    fprintf(stderr, "\n[sql2csv] Done. %d tables, %lld rows, %.1f MB processed in %dm %02ds\n",
            ctx->table_count, (long long)ctx->total_rows, mb, mins, secs);
}

#endif /* PROGRESS_H */
