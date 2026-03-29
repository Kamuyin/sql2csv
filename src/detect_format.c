#include "parser.h"
#include <string.h>
#include <ctype.h>

SqlFormat detect_format(StreamReader *sr) {
    sr_ensure(sr, FORMAT_DETECT_PEEK_SIZE);
    size_t avail = sr_available(sr);
    const char *buf = sr_ptr(sr);
    size_t check = avail < FORMAT_DETECT_PEEK_SIZE ? avail : FORMAT_DETECT_PEEK_SIZE;

    int mysql_score = 0;
    int pgsql_score = 0;

    for (size_t i = 0; i + 10 < check; i++) {
        const char *p = buf + i;

        if (i + 18 <= check && memcmp(p, "-- MySQL dump", 13) == 0) mysql_score += 5;
        if (i + 20 <= check && memcmp(p, "-- phpMyAdmin SQL", 17) == 0) mysql_score += 5;
        if (i + 16 <= check && memcmp(p, "-- Server version", 17) == 0) mysql_score += 2;
        if (p[0] == '`') mysql_score += 1;
        if (i + 12 <= check && memcmp(p, "ENGINE=", 7) == 0) mysql_score += 3;
        if (i + 12 <= check && memcmp(p, "AUTO_INCREMENT", 14) == 0) mysql_score += 3;
        if (i + 8 <= check && memcmp(p, "/*!",  3) == 0) mysql_score += 3;
        if (i + 12 <= check && memcmp(p, "LOCK TABLES",  11) == 0) mysql_score += 3;
        if (i + 14 <= check && memcmp(p, "UNLOCK TABLES", 13) == 0) mysql_score += 2;

        if (i + 18 <= check && memcmp(p, "-- PostgreSQL", 13) == 0) pgsql_score += 5;
        if (i + 10 <= check && memcmp(p, "-- Dumped from", 14) == 0) pgsql_score += 3;
        if (i + 10 <= check && memcmp(p, "pg_dump", 7) == 0) pgsql_score += 5;
        if (i + 10 <= check && memcmp(p, "SET search_path", 15) == 0) pgsql_score += 4;
        if (i + 10 <= check && memcmp(p, "SET client_encoding", 19) == 0) pgsql_score += 3;
        if (i + 16 <= check && (memcmp(p, "COPY ", 5) == 0)) pgsql_score += 3;
        if (i + 12 <= check && memcmp(p, "\\.", 2) == 0) pgsql_score += 4;
        if (i + 20 <= check && memcmp(p, "CREATE SEQUENCE", 15) == 0) pgsql_score += 2;
        if (i + 20 <= check && memcmp(p, "ALTER SEQUENCE", 14) == 0) pgsql_score += 2;
        if (i + 14 <= check && memcmp(p, "SELECT pg_catalog", 17) == 0) pgsql_score += 4;
    }

    if (pgsql_score > mysql_score && pgsql_score >= 3) return FORMAT_PGSQL;
    if (mysql_score > pgsql_score && mysql_score >= 3) return FORMAT_MYSQL;
    if (mysql_score >= 3) return FORMAT_MYSQL;

    return FORMAT_MYSQL;
}
