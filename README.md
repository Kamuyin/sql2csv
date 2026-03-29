# sql2csv

A small utility to convert `.sql` database dumps from MySQL or Postgres to CSV files.

## Usage

```text
sql2csv v1.0.0 - Convert SQL dump files to per-table CSV files

Usage: sql2csv [options] <input.sql>

Options:
  -o, --output <dir>    Output directory (default: current directory)
  -f, --format <fmt>    Force format: mysql, pgsql (default: auto-detect)
  -b, --buffer <size>   Read buffer size in MB (default: 4)
  -v, --verbose         Show progress on stderr
  -h, --help            Show this help message
      --version         Show version

Supported formats:
  mysql   - MySQL / phpMyAdmin / MariaDB dump
  pgsql   - PostgreSQL pg_dump output
```

Example:

```text
sql2csv database.sql -o out/
```

## Building

From windows you can use:

```powershell
.\build.bat
```

Or the usual way:

```bash
mkdir build && cd build
cmake ..
make
```

---

This code was mostly written by GitHub Copilot; It works pretty good  for my database dumps, but I can't guarantee anything. There will be bugs and it might not be able to fully process the statements on certain edge cases.
