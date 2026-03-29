#ifndef TABLE_REGISTRY_H
#define TABLE_REGISTRY_H

#include "types.h"
#include "uthash.h"
#include <stdio.h>

typedef struct TableInfo
{
    char *name;
    char **columns;
    int col_count;
    FILE *csv_fp;
    int64_t row_count;
    UT_hash_handle hh;
} TableInfo;

typedef struct
{
    TableInfo *tables;
    char *output_dir;
    int table_count;
    int64_t total_rows;
} TableRegistry;

AppResult registry_create(TableRegistry *reg, const char *output_dir);

AppResult registry_add_table(TableRegistry *reg, const char *name,
                             char **columns, int col_count);

TableInfo *registry_find(TableRegistry *reg, const char *name);

void registry_destroy(TableRegistry *reg);

#endif /* TABLE_REGISTRY_H */
