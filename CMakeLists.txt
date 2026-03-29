cmake_minimum_required(VERSION 3.20)
project(sql2csv VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# MSVC-specific flags
if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Release optimizations
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Vendor includes (uthash)
include_directories(${CMAKE_SOURCE_DIR}/vendor)

# Main executable
add_executable(sql2csv
    src/main.c
    src/stream_reader.c
    src/lexer.c
    src/csv_writer.c
    src/table_registry.c
    src/parser_mysql.c
    src/parser_pgsql.c
    src/detect_format.c
)

target_include_directories(sql2csv PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Install
install(TARGETS sql2csv RUNTIME DESTINATION bin)
