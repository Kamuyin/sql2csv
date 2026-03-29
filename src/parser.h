#ifndef PARSER_H
#define PARSER_H

#include "types.h"
#include "stream_reader.h"
#include "table_registry.h"
#include "progress.h"

#define FORMAT_DETECT_PEEK_SIZE 8192

SqlFormat detect_format(StreamReader *sr);

AppResult parse_mysql(StreamReader *sr, TableRegistry *reg, ProgressCtx *prog);

AppResult parse_pgsql(StreamReader *sr, TableRegistry *reg, ProgressCtx *prog);

#endif /* PARSER_H */
