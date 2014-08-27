#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "error_stuff.h"
#include "util.h"
#include "utf_tab.h"

struct utf_query_result analyze_utf(reader_t *infile, const long offset, int indent, int print, const struct utf_query *query)
{
    unsigned char buf[4];
    struct utf_table_info table_info;
    char *string_table = NULL;
    struct utf_column_info * schema = NULL;
    struct utf_query_result result;
    
    result.valid = 0;

    if (print)
    {
        fprintf_indent(stdout, indent);
        printf("{\n");
    }

    indent += INDENT_LEVEL;

    table_info.table_offset = offset;

    /* check header */
    static const char UTF_signature[4] = "@UTF"; /* intentionally unterminated */
    get_bytes_seek(offset, infile, buf, 4);
    if (memcmp(buf, UTF_signature, sizeof(UTF_signature)))
    {
        if (print)
        {
            fprintf_indent(stdout, indent);
            printf("not a @UTF table at %08" PRIx32 "\n", (uint32_t)offset);
        }
        goto cleanup;
    }

    /* get table size */
    table_info.table_size = get_32_be(infile);

    table_info.schema_offset = 0x20;
    table_info.rows_offset = get_32_be(infile);
    table_info.string_table_offset = get_32_be(infile);
    table_info.data_offset = get_32_be(infile);
    const uint32_t table_name_string = get_32_be(infile);
    table_info.columns = get_16_be(infile);
    table_info.row_width = get_16_be(infile);
    table_info.rows = get_32_be(infile);

    /* allocate for string table */
    const int string_table_size =
        table_info.data_offset-table_info.string_table_offset;
    string_table = malloc(string_table_size+1);
    CHECK_ERRNO(!string_table, "malloc");
    table_info.string_table = string_table;
    memset(string_table, 0, string_table_size+1);

    /* load schema */
    schema = malloc(sizeof(struct utf_column_info) * table_info.columns);
    CHECK_ERRNO(!schema, "malloc");
    {
        int i;
        for (i = 0; i < table_info.columns; i++)
        {
            schema[i].type = get_byte(infile);
            schema[i].column_name = string_table + get_32_be(infile);

            if ((schema[i].type & COLUMN_STORAGE_MASK) == COLUMN_STORAGE_CONSTANT)
            {
                schema[i].constant_offset = reader_tell(infile);
                switch (schema[i].type & COLUMN_TYPE_MASK)
                {
                    case COLUMN_TYPE_STRING:
                        get_32_be(infile);
                        break;
                    case COLUMN_TYPE_8BYTE:
                    case COLUMN_TYPE_DATA:
                        get_32_be(infile);
                        get_32_be(infile);
                        break;
                    case COLUMN_TYPE_FLOAT:
                    case COLUMN_TYPE_4BYTE2:
                    case COLUMN_TYPE_4BYTE:
                        get_32_be(infile);
                        break;
                    case COLUMN_TYPE_2BYTE2:
                    case COLUMN_TYPE_2BYTE:
                        get_16_be(infile);
                        break;
                    case COLUMN_TYPE_1BYTE2:
                    case COLUMN_TYPE_1BYTE:
                        get_byte(infile);
                        break;
                    default:
                        CHECK_ERROR(1, "unknown type for constant");
                }
            }
        }
    }

    table_info.schema = schema;

    /* read string table */
    get_bytes_seek(table_info.string_table_offset+8+offset,
            infile, (unsigned char *)string_table, string_table_size);
    table_info.table_name = table_info.string_table+table_name_string;

#if 0
    if (print)
    {
        fprintf_table_info(stdout, &table_info, indent);
    }
#endif

    /* fill in the default stuff */
    result.valid = 1;
    result.found = 0;
    result.rows = table_info.rows;
    result.name_offset = table_name_string;
    result.string_table_offset = table_info.string_table_offset;
    result.data_offset = table_info.data_offset;

    /* explore the values */
    if (query || print) {
        int i, j;

        for (i = 0; i < table_info.rows; i++)
        {
            if (!print && query && i != query->index) continue;

            uint32_t row_offset =
                table_info.table_offset + 8 + table_info.rows_offset +
                i * table_info.row_width;
            const uint32_t row_start_offset = row_offset;

            if (print)
            {
                fprintf_indent(stdout, indent);
                printf("%s[%d] = {\n", table_info.table_name, i);
            }
            indent += INDENT_LEVEL;
            for (j = 0; j < table_info.columns; j++)
            {
                uint8_t type = table_info.schema[j].type;
                long constant_offset = table_info.schema[j].constant_offset;
                int constant = 0;

                int qthis = (query && i == query->index &&
                        !strcmp(table_info.schema[j].column_name, query->name));

                if (print)
                {
                    fprintf_indent(stdout, indent);
#if 1
                    printf("%08x %02x %s = ", row_offset-row_start_offset, type, table_info.schema[j].column_name);
#else
                    printf("%s = ", table_info.schema[j].column_name);
#endif
                }

                if (qthis)
                {
                    result.found = 1;
                    result.type = schema[j].type & COLUMN_TYPE_MASK;
                }

                switch (schema[j].type & COLUMN_STORAGE_MASK)
                {
                    case COLUMN_STORAGE_PERROW:
                        break;
                    case COLUMN_STORAGE_CONSTANT:
                        constant = 1;
                        break;
                    case COLUMN_STORAGE_ZERO:
                        if (print)
                        {
                            printf("UNDEFINED\n");
                        }
                        if (qthis)
                        {
                            memset(&result.value, 0,
                                    sizeof(result.value));
                        }
                        continue;
                    default:
                        CHECK_ERROR(1, "unknown storage class");
                }

                if (1)
                {
                    long data_offset;
                    int bytes_read;

                    if (constant)
                    {
                        data_offset = constant_offset;
                        if (print)
                        {
                            printf("constant ");
                        }
                    }
                    else
                    {
                        data_offset = row_offset;
                    }

                    switch (type & COLUMN_TYPE_MASK)
                    {
                        case COLUMN_TYPE_STRING:
                            {
                                uint32_t string_offset;
                                string_offset = get_32_be_seek(data_offset, infile);
                                bytes_read = 4;
                                if (print)
                                {
                                    printf("\"%s\"\n", table_info.string_table + string_offset);
                                }
                                if (qthis)
                                {
                                    result.value.value_string = string_offset;
                                }
                            }
                            break;
                        case COLUMN_TYPE_DATA:
                            {
                                uint32_t vardata_offset, vardata_size;

                                vardata_offset = get_32_be_seek(data_offset, infile);
                                vardata_size = get_32_be(infile);
                                bytes_read = 8;
                                if (print)
                                {
                                    printf("[0x%08" PRIx32 "]", vardata_offset);
                                    printf(" (size 0x%08" PRIx32 ")\n", vardata_size);
                                }
                                if (qthis)
                                {
                                    result.value.value_data.offset = vardata_offset;
                                    result.value.value_data.size = vardata_size;
                                }

                                if (vardata_size != 0 && print)
                                {
                                    /* assume that the data is another table */
                                    analyze_utf(infile,
                                            table_info.table_offset + 8 +
                                            table_info.data_offset +
                                            vardata_offset,
                                            indent,
                                            print,
                                            NULL
                                            );
                                }
                            }
                            break;

                        case COLUMN_TYPE_8BYTE:
                            {
                                uint64_t value =
                                    get_64_be_seek(data_offset, infile);
                                if (print)
                                {
                                    printf("0x%" PRIx64 "\n", value);
                                }
                                if (qthis)
                                {
                                    result.value.value_u64 = value;
                                }
                                bytes_read = 8;
                                break;
                            }
                        case COLUMN_TYPE_4BYTE2:
                            if (print)
                            {
                                printf("type 2 ");
                            }
                        case COLUMN_TYPE_4BYTE:
                            {
                                uint32_t value =
                                    get_32_be_seek(data_offset, infile);
                                if (print)
                                {
                                    printf("%" PRId32 "\n", value);
                                }
                                if (qthis)
                                {
                                    result.value.value_u32 = value;
                                }
                                bytes_read = 4;
                            }
                            break;
                        case COLUMN_TYPE_2BYTE2:
                            if (print)
                            {
                                printf("type 2 ");
                            }
                        case COLUMN_TYPE_2BYTE:
                            {
                                uint16_t value = 
                                    get_16_be_seek(data_offset, infile);
                                if (print)
                                {
                                    printf("%" PRId16 "\n", value);
                                }
                                if (qthis)
                                {
                                    result.value.value_u16 = value;
                                }
                                bytes_read = 2;
                            }
                            break;
                        case COLUMN_TYPE_FLOAT:
                            if (sizeof(float) == 4)
                            {
                                union {
                                    float float_value;
                                    uint32_t int_value;
                                } int_float;

                                int_float.int_value = get_32_be_seek(data_offset, infile);
                                if (print)
                                {
                                    printf("%f\n", int_float.float_value);
                                }
                                if (qthis)
                                {
                                    result.value.value_float = int_float.float_value;
                                }
                            }
                            else
                            {
                                get_32_be_seek(data_offset, infile);
                                if (print)
                                {
                                    printf("float\n");
                                }
                                if (qthis)
                                {
                                    CHECK_ERROR(1, "float is wrong size, can't return");
                                }
                            }
                            bytes_read = 4;
                            break;
                        case COLUMN_TYPE_1BYTE2:
                            if (print)
                            {
                                printf("type 2 ");
                            }
                        case COLUMN_TYPE_1BYTE:
                            {
                                uint8_t value =
                                    get_byte_seek(data_offset, infile);
                                if (print)
                                {
                                    printf("%" PRId8 "\n", value);
                                }
                                if (qthis)
                                {
                                    result.value.value_u8 = value;
                                }
                                bytes_read = 1;
                            }
                            break;
                        default:
                            CHECK_ERROR(1, "unknown normal type");
                    }

                    if (!constant)
                    {
                        row_offset += bytes_read;
                    }
                } /* useless if end */
            } /* column for loop end */
            indent -= INDENT_LEVEL;
            if (print)
            {
                fprintf_indent(stdout,indent);
                printf("}\n");
            }

            CHECK_ERROR(row_offset - row_start_offset != table_info.row_width,
                    "column widths do now add up to row width");

            if (query && !print && i >= query->index) break;
        } /* row for loop end */
    } /* explore values block end */

cleanup:
    indent -= INDENT_LEVEL;
    if (print)
    {
        fprintf_indent(stdout, indent);
        printf("}\n");
    }

    if (string_table)
    {
        free(string_table);
        string_table = NULL;
    }

    if (schema)
    {
        free(schema);
        schema = NULL;
    }

    return result;
}

void fprintf_table_info(FILE *outfile, const struct utf_table_info *table_info, int indent)
{
    fprintf_indent(outfile,indent);
    fprintf(outfile, "table: %s\n", table_info->table_name);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "at offset           0x%08" PRIx32 " (size 0x%08" PRIx32 ")\n",
            (uint32_t)table_info->table_offset,
            table_info->table_size);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "schema offset       0x%08" PRIx32 " (size 0x%08" PRIx32 ")\n",
            table_info->schema_offset,
            table_info->rows_offset-table_info->schema_offset);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "rows offset         0x%08" PRIx32 " (size 0x%08" PRIx32 ")\n",
            table_info->rows_offset,
            table_info->string_table_offset-table_info->rows_offset);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "string table offset 0x%08" PRIx32 " (size 0x%08" PRIx32 ")\n",
            table_info->string_table_offset,
            table_info->data_offset-table_info->string_table_offset);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "data offset         0x%08" PRIx32 " (0x%08" PRIx32 ")\n",
            table_info->data_offset,
            table_info->table_size-table_info->data_offset);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "columns:            %" PRIu32 "\n",
            table_info->columns);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "rows:               %" PRIu32 "\n",
            table_info->rows);
    fprintf_indent(outfile,indent);
    fprintf(outfile, "row width:          %#" PRIx32 " bytes\n",
            table_info->row_width * table_info->rows);
}

struct utf_query_result query_utf(reader_t *infile, const long offset, const struct utf_query *query)
{
    return analyze_utf(infile, offset, 0, 0, query);
}

struct utf_query_result query_utf_nofail(reader_t *infile, const long offset, const struct utf_query *query)
{
    const struct utf_query_result result = query_utf(infile, offset, query);

    CHECK_ERROR (!result.valid, "didn't find valid @UTF table where one was expected");
    CHECK_ERROR (query && !result.found, "key not found");

    return result;
}

struct utf_query_result query_utf_key(reader_t *infile, const long offset, int index, const char *name)
{
    struct utf_query query;
    query.index = index;
    query.name = name;

    return query_utf_nofail(infile, offset, &query);
}

uint64_t query_utf_8byte(reader_t *infile, const long offset, int index, const char *name)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name);
    CHECK_ERROR(result.type != COLUMN_TYPE_8BYTE, "value is not an 8 byte uint");
    return result.value.value_u64;
}

uint32_t query_utf_4byte(reader_t *infile, const long offset, int index, const char *name)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name);
    CHECK_ERROR(result.type != COLUMN_TYPE_4BYTE, "value is not a 4 byte uint");
    return result.value.value_u32;
}

uint16_t query_utf_2byte(reader_t *infile, const long offset, int index, const char *name)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name);
    CHECK_ERROR(result.type != COLUMN_TYPE_2BYTE, "value is not a 2 byte uint");
    return result.value.value_u16;
}

char *load_utf_string_table(reader_t *infile, const long offset)
{
    const struct utf_query_result result = query_utf_nofail(infile, offset, NULL);

    const size_t string_table_size = result.data_offset - result.string_table_offset;
    const long string_table_offset = offset + 8 + result.string_table_offset;
    char *string_table = malloc(string_table_size + 1);

    CHECK_ERRNO (!string_table, "malloc");
    memset(string_table, 0, string_table_size+1);
    get_bytes_seek(string_table_offset, infile,
            (unsigned char *)string_table, string_table_size);

    return string_table;
}

void free_utf_string_table(char *string_table)
{
    free(string_table);
}

const char *query_utf_string(reader_t *infile, const long offset,
        int index, const char *name, const char *string_table)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name);
    CHECK_ERROR(result.type != COLUMN_TYPE_STRING, "value is not a string");
    return string_table + result.value.value_string;
}

struct offset_size_pair query_utf_data(reader_t *infile, const long offset,
        int index, const char *name)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name);
    CHECK_ERROR(result.type != COLUMN_TYPE_DATA, "value is not data");
    return result.value.value_data;
}
