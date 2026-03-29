#include "table_registry.h"
#include "csv_writer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* uthash macros trigger C4702 (unreachable code) on MSVC */
#ifdef _MSC_VER
#pragma warning(disable: 4702)
#endif

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#define mkdir_p(path) mkdir(path, 0755)
#define PATH_SEP '/'
#endif

static void sanitize_filename(char *buf, size_t bufsize, const char *name) {
    size_t len = strlen(name);
    if (len >= bufsize) len = bufsize - 1;
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == '\0' || (unsigned char)c < 32) {
            buf[i] = '_';
        } else {
            buf[i] = c;
        }
    }
    buf[len] = '\0';
}

AppResult registry_create(TableRegistry *reg, const char *output_dir) {
    memset(reg, 0, sizeof(*reg));
    reg->output_dir = _strdup(output_dir);
    if (!reg->output_dir) return ERR_MEMORY;
    reg->tables = NULL;
    reg->table_count = 0;
    reg->total_rows = 0;
    return OK;
}

AppResult registry_add_table(TableRegistry *reg, const char *name,
                             char **columns, int col_count) {
    TableInfo *existing = NULL;
    HASH_FIND_STR(reg->tables, name, existing);
    if (existing) {
        /* table seen before; re-open with the new column schema */
            if (existing->csv_fp) {
            fclose(existing->csv_fp);
            existing->csv_fp = NULL;
        }
        for (int i = 0; i < existing->col_count; i++) {
            free(existing->columns[i]);
        }
        free(existing->columns);
        existing->row_count = 0;

        existing->columns = (char **)malloc(sizeof(char *) * col_count);
        if (!existing->columns) return ERR_MEMORY;
        existing->col_count = col_count;
        for (int i = 0; i < col_count; i++) {
            existing->columns[i] = _strdup(columns[i]);
            if (!existing->columns[i]) return ERR_MEMORY;
        }

        char safe_name[512];
        sanitize_filename(safe_name, sizeof(safe_name), name);
        char path[1024];
        snprintf(path, sizeof(path), "%s%c%s.csv", reg->output_dir, PATH_SEP, safe_name);
        existing->csv_fp = csv_open(path);
        if (!existing->csv_fp) return ERR_IO;
        csv_write_header(existing->csv_fp, existing->columns, existing->col_count);
        return OK;
    }

    TableInfo *info = (TableInfo *)calloc(1, sizeof(TableInfo));
    if (!info) return ERR_MEMORY;

    info->name = _strdup(name);
    if (!info->name) { free(info); return ERR_MEMORY; }

    info->columns = (char **)malloc(sizeof(char *) * col_count);
    if (!info->columns) { free(info->name); free(info); return ERR_MEMORY; }

    info->col_count = col_count;
    for (int i = 0; i < col_count; i++) {
        info->columns[i] = _strdup(columns[i]);
        if (!info->columns[i]) return ERR_MEMORY;
    }

    char safe_name[512];
    sanitize_filename(safe_name, sizeof(safe_name), name);
    char path[1024];
    snprintf(path, sizeof(path), "%s%c%s.csv", reg->output_dir, PATH_SEP, safe_name);

    info->csv_fp = csv_open(path);
    if (!info->csv_fp) {
        for (int i = 0; i < col_count; i++) free(info->columns[i]);
        free(info->columns);
        free(info->name);
        free(info);
        return ERR_IO;
    }

    csv_write_header(info->csv_fp, info->columns, info->col_count);

    info->row_count = 0;
    HASH_ADD_STR(reg->tables, name, info);
    reg->table_count++;
    return OK;
}

TableInfo *registry_find(TableRegistry *reg, const char *name) {
    TableInfo *info = NULL;
    HASH_FIND_STR(reg->tables, name, info);
    return info;
}

void registry_destroy(TableRegistry *reg) {
    TableInfo *info, *tmp;
    HASH_ITER(hh, reg->tables, info, tmp) {
        HASH_DEL(reg->tables, info);
        if (info->csv_fp) fclose(info->csv_fp);
        for (int i = 0; i < info->col_count; i++) {
            free(info->columns[i]);
        }
        free(info->columns);
        free(info->name);
        free(info);
    }
    free(reg->output_dir);
    reg->output_dir = NULL;
    reg->tables = NULL;
    reg->table_count = 0;
}
