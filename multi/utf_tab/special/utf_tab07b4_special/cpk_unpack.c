#include <stdio.h>
#include <limits.h>

#include "utf_tab.h"
#include "cpk_uncompress.h"
#include "util.h"
#include "error_stuff.h"

void analyze_CPK(reader_t *infile, const char *base_name, long file_length);

int main(int argc, char **argv)
{
    printf("cpk_unpack " VERSION "\n\n");
    if (argc != 2)
    {
        fprintf(stderr,"Incorrect program usage\n\nusage: %s file\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* open file */
    reader_t *infile = open_reader_file(argv[1]);
    CHECK_ERRNO(!infile, "fopen");

    const char *base_postfix = "_unpacked";
    char *base_name = malloc(strlen(argv[1])+strlen(base_postfix)+1);
    CHECK_ERRNO(!base_name, "malloc");
    strcpy(base_name, strip_path(argv[1]));
    strcat(base_name, base_postfix);

    long file_length = reader_length(infile);

    analyze_CPK(infile, base_name, file_length);

    free(base_name);
    close_reader(infile);

    exit(EXIT_SUCCESS);
}

void analyze_CPK(reader_t *infile, const char *base_name, long file_length)
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

    long toc_offset, content_offset, CpkHeader_count;
    {
        reader_t *cpk_crypt_infile = open_reader_crypt(infile, CpkHeader_offset+0x10, 0x5f, 0x15);

        /* check CpkHeader */
        {
            struct utf_query_result result = query_utf_nofail(cpk_crypt_infile, CpkHeader_offset+0x10, NULL);

            CHECK_ERROR (result.rows != 1, "wrong number of rows in CpkHeader");
        }

        /* get TOC offset */
        toc_offset = query_utf_8byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "TocOffset");

        /* get content offset */
        content_offset = query_utf_8byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "ContentOffset");

        /* get file count from CpkHeader */
        CpkHeader_count = query_utf_4byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "Files");

        close_reader(cpk_crypt_infile);
    }

    {
        reader_t *toc_crypt_infile = open_reader_crypt(infile, toc_offset+0x10, 0x5f, 0x15);

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
            struct utf_query_result result = query_utf_nofail(toc_crypt_infile, toc_offset+0x10, NULL);

            toc_entries = result.rows;
            toc_string_table = load_utf_string_table(toc_crypt_infile, toc_offset+0x10);
        }

        /* check that counts match */
        CHECK_ERROR( toc_entries != CpkHeader_count, "CpkHeader file count and TOC entry count do not match" );

        /* extract files */
        for (int i = 0; i < toc_entries; i++)
        {
#if 1
            reader_t *data_crypt_infile = NULL;
#else
            reader_t *data_crypt_infile = infile;
#endif

            /* get file name */
            const char *file_name = query_utf_string(toc_crypt_infile, toc_offset+0x10, i,
                    "FileName", toc_string_table);

            /* get directory name */
            const char *dir_name = query_utf_string(toc_crypt_infile, toc_offset+0x10, i,
                    "DirName", toc_string_table);

            /* get file size */
            long file_size = query_utf_4byte(toc_crypt_infile, toc_offset+0x10, i,
                    "FileSize");

            /* get extract size */
            long extract_size = query_utf_4byte(toc_crypt_infile, toc_offset+0x10, i,
                    "ExtractSize");

            /* get file offset */
            uint64_t file_offset_raw = 
                query_utf_8byte(toc_crypt_infile, toc_offset+0x10, i, "FileOffset");
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

#if 1
            data_crypt_infile = open_reader_crypt(infile, file_offset, 0x5f, 0x15);
#endif

            if (extract_size > file_size)
            {
                long uncompressed_size =
                    uncompress(data_crypt_infile, file_offset, file_size, outfile);
                printf("   uncompressed to %ld\n", uncompressed_size);

                CHECK_ERROR( uncompressed_size != extract_size ,
                        "uncompressed size != ExtractSize");
            }
            else
            {
                dump(data_crypt_infile, outfile, file_offset, file_size);
            }
            CHECK_ERRNO(fclose(outfile) != 0, "fclose");

#if 1
            close_reader(data_crypt_infile);
#endif
        }

        close_reader(toc_crypt_infile);
    }

    if (toc_string_table)
    {
        free_utf_string_table(toc_string_table);
        toc_string_table = NULL;
    }
}
