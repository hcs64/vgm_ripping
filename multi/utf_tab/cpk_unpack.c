#include <stdio.h>
#include <limits.h>

#include "utf_tab.h"
#include "cpk_uncompress.h"
#include "util.h"
#include "error_stuff.h"

void analyze_CPK(FILE *infile, const char *base_name, long file_length);

int main(int argc, char **argv)
{
    printf("cpk_unpack " VERSION "\n\n");
    if (argc != 2)
    {
        fprintf(stderr,"Incorrect program usage\n\nusage: %s file\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen");

    const char *base_postfix = "_unpacked";
    char *base_name = malloc(strlen(argv[1])+strlen(base_postfix)+1);
    CHECK_ERRNO(!base_name, "malloc");
    strcpy(base_name, strip_path(argv[1]));
    strcat(base_name, base_postfix);

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    analyze_CPK(infile, base_name, file_length);

    free(base_name);

    exit(EXIT_SUCCESS);
}

void analyze_CPK(FILE *infile, const char *base_name, long file_length)
{
    const long CpkHeader_offset = 0x0;
    char *toc_string_table = NULL;

    /* check header */
    {
        unsigned char buf[4];
        static const char CPK_signature[4] = "CPK "; /* intentionally unterminated */
        get_bytes_seek(CpkHeader_offset, infile, buf, 4);
        CHECK_ERROR (memcmp(buf, CPK_signature, sizeof(CPK_signature)), "CPK signature not found");
    }

    /* check CpkHeader */
    {
        struct utf_query_result result = query_utf_nofail(infile, CpkHeader_offset+0x10, NULL);

        CHECK_ERROR (result.rows != 1, "wrong number of rows in CpkHeader");
    }

    /* get TOC offset */
    long toc_offset = query_utf_8byte(infile, CpkHeader_offset+0x10, 0, "TocOffset");

    /* get content offset */
    long content_offset = query_utf_8byte(infile, CpkHeader_offset+0x10, 0, "ContentOffset");

    /* get file count from CpkHeader */
    long CpkHeader_count = query_utf_4byte(infile, CpkHeader_offset+0x10, 0, "Files");

    /* check TOC header */
    {
        unsigned char buf[4];
        static const char TOC_signature[4] = "TOC "; /* intentionally unterminated */
        get_bytes_seek(toc_offset, infile, buf, 4);
        CHECK_ERROR (memcmp(buf, TOC_signature, sizeof(TOC_signature)), "TOC signature not found");
    }

    /* get TOC entry count, string table */
    long toc_entries;
    {
        struct utf_query_result result = query_utf_nofail(infile, toc_offset+0x10, NULL);

        toc_entries = result.rows;
        toc_string_table = load_utf_string_table(infile, toc_offset+0x10);
    }

    /* check that counts match */
    CHECK_ERROR( toc_entries != CpkHeader_count, "CpkHeader file count and TOC entry count do not match" );

    /* extract files */
    for (int i = 0; i < toc_entries; i++)
    {
        /* get file name */
        const char *file_name = query_utf_string(infile, toc_offset+0x10, i,
                "FileName", toc_string_table);

        /* get directory name */
        const char *dir_name = query_utf_string(infile, toc_offset+0x10, i,
                "DirName", toc_string_table);

        /* get file size */
        long file_size = query_utf_4byte(infile, toc_offset+0x10, i,
                "FileSize");

        /* get extract size */
        long extract_size = query_utf_4byte(infile, toc_offset+0x10, i,
                "ExtractSize");

        /* get file offset */
        uint64_t file_offset_raw = 
            query_utf_8byte(infile, toc_offset+0x10, i, "FileOffset");
        if (content_offset < toc_offset)
        {
            file_offset_raw += content_offset;
        }
        else
        {
            file_offset_raw += toc_offset;
        }

        CHECK_ERROR( file_offset_raw > LONG_MAX, "File offset too large, will be unable to seek" );
        long file_offset = file_offset_raw;

        printf("%s/%s 0x%lx %ld\n",
                dir_name, file_name, (unsigned long)file_offset, file_size);
        FILE *outfile = open_file_in_directory(base_name, dir_name, '/', file_name, "w+b");
        CHECK_ERRNO(!outfile, "fopen");

        if (extract_size > file_size)
        {
            long uncompressed_size =
                uncompress(infile, file_offset, file_size, outfile);
            printf("   uncompressed to %ld\n", uncompressed_size);
            
            CHECK_ERROR( uncompressed_size != extract_size ,
                    "uncompressed size != ExtractSize");
        }
        else
        {
            dump(infile, outfile, file_offset, file_size);
        }
        CHECK_ERRNO(fclose(outfile) != 0, "fclose");
    }

    if (toc_string_table)
    {
        free_utf_string_table(toc_string_table);
        toc_string_table = NULL;
    }
}
