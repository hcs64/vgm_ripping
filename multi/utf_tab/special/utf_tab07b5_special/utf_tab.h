#ifndef _UTF_TAB_H_INCLUDED
#define _UTF_TAB_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "error_stuff.h"
#include "util.h"

/* common version across the suite */
#define VERSION "0.7 beta 5 for over.cpk"

struct utf_query
{
    /* if 0 */
    const char *name;
    int index;
};

struct offset_size_pair
{
    uint32_t offset;
    uint32_t size;
};

struct utf_query_result
{
    int valid;  /* table is valid */
    int found;
    int type;   /* one of COLUMN_TYPE_* */
    union
    {
        uint64_t value_u64;
        uint32_t value_u32;
        uint16_t value_u16;
        uint8_t value_u8;
        float value_float;
        struct offset_size_pair value_data;
        uint32_t value_string;
    } value;

    /* info for the queried table */
    uint32_t rows;
    uint32_t name_offset;
    uint32_t string_table_offset;
    uint32_t data_offset;
};

struct utf_query_result analyze_utf(reader_t *infile, long offset, int indent,
        int print, const struct utf_query *query);

struct utf_query_result query_utf(reader_t *infile, long offset,
        const struct utf_query *query);

struct utf_query_result query_utf_nofail(reader_t *infile, const long offset,
        const struct utf_query *query);

struct utf_query_result query_utf_key(reader_t *infile, const long offset,
        int index, const char *name);

uint64_t query_utf_8byte(reader_t *infile, const long offset,
        int index, const char *name);

uint32_t query_utf_4byte(reader_t *infile, const long offset,
        int index, const char *name);

uint16_t query_utf_2byte(reader_t *infile, const long offset,
        int index, const char *name);

char *load_utf_string_table(reader_t *infile, const long offset);

void free_utf_string_table(char *string_table);

const char *query_utf_string(reader_t *infile, const long offset,
        int index, const char *name, const char *string_table);

struct offset_size_pair query_utf_data(reader_t *infile, const long offset,
        int index, const char *name);

#define COLUMN_STORAGE_MASK         0xf0
#define COLUMN_STORAGE_PERROW       0x50
#define COLUMN_STORAGE_CONSTANT     0x30
#define COLUMN_STORAGE_ZERO         0x10

/* I suspect that "type 2" is signed */
#define COLUMN_TYPE_MASK            0x0f
#define COLUMN_TYPE_DATA            0x0b
#define COLUMN_TYPE_STRING          0x0a
/* 0x09 double? */
#define COLUMN_TYPE_FLOAT           0x08
/* 0x07 signed 8byte? */
#define COLUMN_TYPE_8BYTE           0x06
#define COLUMN_TYPE_4BYTE2          0x05
#define COLUMN_TYPE_4BYTE           0x04
#define COLUMN_TYPE_2BYTE2          0x03
#define COLUMN_TYPE_2BYTE           0x02
#define COLUMN_TYPE_1BYTE2          0x01
#define COLUMN_TYPE_1BYTE           0x00

struct utf_column_info
{
    uint8_t type;
    const char *column_name;
    long constant_offset;
};

struct utf_table_info
{
    long table_offset;
    uint32_t table_size;
    uint32_t schema_offset;
    uint32_t rows_offset;
    uint32_t string_table_offset;
    uint32_t data_offset;
    const char *string_table;
    const char *table_name;
    uint16_t columns;
    uint16_t row_width;
    uint32_t rows;

    const struct utf_column_info *schema;
};

void fprintf_table_info(FILE *outfile, const struct utf_table_info *table_info, int indent);

#endif /* _UTF_TAB_H_INCLUDED */
