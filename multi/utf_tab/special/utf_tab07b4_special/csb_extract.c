#include <stdio.h>
#include <stdlib.h>

#include "utf_tab.h"
#include "util.h"
#include "error_stuff.h"

void analyze_CSB(reader_t *infile, long file_length);

int main(int argc, char **argv)
{
    printf("csb_extract " VERSION "\n\n");
    if (argc != 2)
    {
        fprintf(stderr,"Incorrect program usage\n\nusage: %s file\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* open file */
    reader_t *infile = open_reader_file(argv[1]);
    CHECK_ERRNO(!infile, "fopen");

    long file_length = reader_length(infile);

    analyze_CSB(infile, file_length);

    close_reader(infile);

    exit(EXIT_SUCCESS);
}

void analyze_CSB(reader_t *infile, long file_length)
{
    const long TBLCSB_offset = 0x0;
    char *csb_string_table = NULL;
    char *sdl_string_table = NULL;

    /* get TBLCSB entry count, string table, data offset */
    int csb_rows;
    long csb_data_offset;
    {
        struct utf_query_result result = query_utf_nofail(infile, TBLCSB_offset, NULL);
        csb_rows = result.rows;
        csb_data_offset = TBLCSB_offset + 8 + result.data_offset;

        /* load string table */
        csb_string_table = load_utf_string_table(infile, TBLCSB_offset);

        /* check that this is in fact a TBLCSB table */
        CHECK_ERROR(strcmp(csb_string_table + result.name_offset,
                    "TBLCSB"), "first table in file is not TBLCSB");
    }

    /* find entry for sound elements */
    int csb_sdl_index;
    {
        for (csb_sdl_index = 0; csb_sdl_index < csb_rows; csb_sdl_index++)
        {
            if (!strcmp(query_utf_string(infile, TBLCSB_offset,
                    csb_sdl_index, "name", csb_string_table),
                        "SOUND_ELEMENT"))
            {
                break;
            }

        }

        CHECK_ERROR(csb_sdl_index >= csb_rows, "SOUND_ELEMENT not found");
    }
    
    /* get sound element table offset */
    long sdl_offset = csb_data_offset + query_utf_data(infile, TBLCSB_offset, csb_sdl_index, "utf").offset;

    /* get sound element entry count, string table, data offset */
    int sdl_rows;
    long sdl_data_offset;
    {
        struct utf_query_result result = query_utf_nofail(infile, sdl_offset, NULL);
        sdl_rows = result.rows;
        sdl_data_offset = sdl_offset + 8 + result.data_offset;

        /* load string table */
        sdl_string_table = load_utf_string_table(infile, sdl_offset);

        /* check that this is in fact a TBLSDL table */
        CHECK_ERROR(strcmp(sdl_string_table + result.name_offset,
                    "TBLSDL"), "SOUND_ELEMENT table in is not TBLSDL");
    }

    /* extract files */
    for (int i = 0; i < sdl_rows; i++)
    {
        /* get file name */
        const char *file_name = query_utf_string(infile, sdl_offset, i,
                "name", sdl_string_table);

        /* get file size and offset */
        struct offset_size_pair offset_size =
            query_utf_data(infile, sdl_offset, i, "data");
        long file_offset = sdl_data_offset + offset_size.offset;
        long file_size = offset_size.size;

        if (file_size == 0) {
            printf("%s size 0\n", file_name);
            continue;
        }

        /* generate a usable filename */
        FILE *outfile = NULL;
        {
            char *out_file_name = malloc(strlen(file_name)+4+1); /* room for ext */
            CHECK_ERRNO(!out_file_name, "malloc");
            strcpy(out_file_name, file_name);
            for (int j = 0; out_file_name[j]; j++)
            {
                if (out_file_name[j] == '/')
                    out_file_name[j] = '_';
            }

            /* check type, add extension */
            do {
                char *file_string_table = NULL;
                struct utf_query_result result = query_utf(infile, file_offset, NULL);

                if (!result.valid) break;

                /* load string table */
                file_string_table = load_utf_string_table(infile, file_offset);

                if (!strcmp(file_string_table + result.name_offset, "AAX"))
                {
                    strcat(out_file_name, ".aax");
                }
                free_utf_string_table(file_string_table);
            } while (0);

            printf("%s %lx %ld\n", out_file_name, (unsigned long)file_offset, file_size);
            outfile = fopen(out_file_name, "wb");
            CHECK_ERRNO(!outfile, "fopen");

            free(out_file_name);
        }

        dump(infile, outfile, file_offset, file_size);
        CHECK_ERRNO(fclose(outfile) != 0, "fclose");
    }

    if (csb_string_table)
    {
        free_utf_string_table(csb_string_table);
        csb_string_table = NULL;
    }

    if (sdl_string_table)
    {
        free_utf_string_table(sdl_string_table);
        sdl_string_table = NULL;
    }
}
