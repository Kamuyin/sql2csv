#include "types.h"
#include "stream_reader.h"
#include "table_registry.h"
#include "parser.h"
#include "progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#define mkdir_p(d) _mkdir(d)
#else
#include <sys/stat.h>
#define mkdir_p(d) mkdir(d, 0755)
#endif


typedef struct {
    const char *input_file;
    const char *output_dir;
    SqlFormat   format;
    size_t      buffer_size;
    int         verbose;
    int         show_help;
    int         show_version;
} CliArgs;

static void print_usage(void) {
    fprintf(stderr,
        "sql2csv v" TOOL_VERSION " - Convert SQL dump files to per-table CSV files\n"
        "\n"
        "Usage: sql2csv [options] <input.sql>\n"
        "\n"
        "Options:\n"
        "  -o, --output <dir>    Output directory (default: current directory)\n"
        "  -f, --format <fmt>    Force format: mysql, pgsql (default: auto-detect)\n"
        "  -b, --buffer <size>   Read buffer size in MB (default: 4)\n"
        "  -v, --verbose         Show progress on stderr\n"
        "  -h, --help            Show this help message\n"
        "      --version         Show version\n"
        "\n"
        "Supported formats:\n"
        "  mysql   - MySQL / phpMyAdmin / MariaDB dump\n"
        "  pgsql   - PostgreSQL pg_dump output\n"
    );
}

static int streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static AppResult parse_args(int argc, char *argv[], CliArgs *args) {
    memset(args, 0, sizeof(*args));
    args->output_dir = ".";
    args->format = FORMAT_UNKNOWN;
    args->buffer_size = BUF_SIZE_DEFAULT;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (streq(arg, "-h") || streq(arg, "--help")) {
            args->show_help = 1;
            return OK;
        }
        if (streq(arg, "--version")) {
            args->show_version = 1;
            return OK;
        }
        if (streq(arg, "-v") || streq(arg, "--verbose")) {
            args->verbose = 1;
            continue;
        }
        if (streq(arg, "-o") || streq(arg, "--output")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return ERR_ARGS;
            }
            args->output_dir = argv[++i];
            continue;
        }
        if (streq(arg, "-f") || streq(arg, "--format")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return ERR_ARGS;
            }
            i++;
            if (streq(argv[i], "mysql")  || streq(argv[i], "MySQL") ||
                streq(argv[i], "mariadb")) {
                args->format = FORMAT_MYSQL;
            } else if (streq(argv[i], "pgsql")  || streq(argv[i], "postgres") ||
                       streq(argv[i], "postgresql") || streq(argv[i], "pg")) {
                args->format = FORMAT_PGSQL;
            } else {
                fprintf(stderr, "Error: unknown format '%s'. Use: mysql, pgsql\n",
                        argv[i]);
                return ERR_FORMAT;
            }
            continue;
        }
        if (streq(arg, "-b") || streq(arg, "--buffer")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return ERR_ARGS;
            }
            i++;
            int mb = atoi(argv[i]);
            if (mb < 1 || mb > 1024) {
                fprintf(stderr, "Error: buffer size must be 1-1024 MB\n");
                return ERR_ARGS;
            }
            args->buffer_size = (size_t)mb * 1024 * 1024;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", arg);
            return ERR_ARGS;
        }

        if (args->input_file) {
            fprintf(stderr, "Error: multiple input files not supported\n");
            return ERR_ARGS;
        }
        args->input_file = arg;
    }

    if (!args->show_help && !args->show_version && !args->input_file) {
        fprintf(stderr, "Error: no input file specified\n");
        return ERR_ARGS;
    }

    return OK;
}


int main(int argc, char *argv[]) {
    CliArgs args;
    AppResult res = parse_args(argc, argv, &args);
    if (res != OK) {
        print_usage();
        return (int)res;
    }

    if (args.show_help) {
        print_usage();
        return 0;
    }
    if (args.show_version) {
        printf("sql2csv v%s\n", TOOL_VERSION);
        return 0;
    }

    {
        struct stat st;
        if (stat(args.output_dir, &st) != 0) {
            if (mkdir_p(args.output_dir) != 0) {
                fprintf(stderr, "Error: cannot create output directory '%s'\n",
                        args.output_dir);
                return (int)ERR_IO;
            }
        }
    }

    StreamReader sr;
    res = sr_open(&sr, args.input_file, (int)(args.buffer_size / (1024 * 1024)));
    if (res != OK) {
        fprintf(stderr, "Error: cannot open '%s'\n", args.input_file);
        return (int)res;
    }

    SqlFormat format = args.format;
    if (format == FORMAT_UNKNOWN) {
        format = detect_format(&sr);
    }

    ProgressCtx prog;
    progress_init(&prog, sr.file_size, args.verbose);

    if (args.verbose) {
        const char *fmt_name = (format == FORMAT_PGSQL) ? "PostgreSQL" : "MySQL";
        fprintf(stderr, "Format:  %s\n", fmt_name);
        fprintf(stderr, "Input:   %s\n", args.input_file);
        fprintf(stderr, "Output:  %s\n", args.output_dir);
        fprintf(stderr, "Buffer:  %zu MB\n", args.buffer_size / (1024 * 1024));
        fprintf(stderr, "---\n");
    }

    TableRegistry reg;
    res = registry_create(&reg, args.output_dir);
    if (res != OK) {
        fprintf(stderr, "Error: out of memory\n");
        sr_close(&sr);
        return (int)res;
    }

    if (format == FORMAT_PGSQL) {
        res = parse_pgsql(&sr, &reg, &prog);
    } else {
        res = parse_mysql(&sr, &reg, &prog);
    }

    if (res != OK) {
        fprintf(stderr, "\nError during parsing (code %d)\n", (int)res);
    }

    int64_t bytes_processed = sr.total_read - (int64_t)sr_available(&sr);
    progress_finish(&prog, bytes_processed);

    if (args.verbose || res == OK) {
        fprintf(stderr, "Tables:  %d\n", reg.table_count);
        fprintf(stderr, "Rows:    %lld\n", (long long)reg.total_rows);
    }

    registry_destroy(&reg);
    sr_close(&sr);

    return (res == OK) ? 0 : (int)res;
}
