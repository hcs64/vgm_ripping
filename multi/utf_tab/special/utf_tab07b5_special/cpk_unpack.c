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

    long toc_offset = 0, itoc_offset = 0, content_offset, CpkHeader_count;
    int align;
    {
        reader_t *cpk_crypt_infile = open_reader_crypt(infile, CpkHeader_offset+0x10, 0x5f, 0x15);

        /* check CpkHeader */
        {
            struct utf_query_result result = query_utf_nofail(cpk_crypt_infile, CpkHeader_offset+0x10, NULL);

            CHECK_ERROR (result.rows != 1, "wrong number of rows in CpkHeader");
        }

        /* get TOC offset */
        toc_offset = query_utf_8byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "TocOffset");
        if (toc_offset == 0)
        {
            itoc_offset = query_utf_8byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "ItocOffset");
            CHECK_ERROR ((itoc_offset == 0), "neither TOC nor ITOC offset found");
        }

        /* get content offset */
        content_offset = query_utf_8byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "ContentOffset");

        /* get file count from CpkHeader */
        CpkHeader_count = query_utf_4byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "Files");

        /* get alignment */
        align = query_utf_2byte(cpk_crypt_infile, CpkHeader_offset+0x10, 0, "Align");

        close_reader(cpk_crypt_infile);
    }

    if (toc_offset)
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
#ifdef DECRYPT_BODY
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

#ifdef DECRYPT_BODY
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

#ifdef DECRYPT_BODY
            close_reader(data_crypt_infile);
#endif
        }

        close_reader(toc_crypt_infile);
    }
    else if (itoc_offset)
    {
        reader_t *itoc_crypt_infile = open_reader_crypt(infile, itoc_offset+0x10, 0x5f, 0x15);

        printf("Using ITOC, no names available\n\n");

        /* check ITOC header */
        {
            unsigned char buf[4];
            static const char ITOC_signature[4] = "ITOC"; /* intentionally unterminated */
            get_bytes_seek(itoc_offset, infile, buf, 4);
            CHECK_ERROR (memcmp(buf, ITOC_signature, sizeof(ITOC_signature)), "ITOC signature not found");
        }

        /* get ITOC info */
        long itoc_entries, itoc_filesl, itoc_filesh, datal_offset, datah_offset;
        {
            struct utf_query_result result;
            
            result = query_utf_nofail(itoc_crypt_infile, itoc_offset+0x10, NULL);

            itoc_entries = result.rows;
            CHECK_ERROR( itoc_entries != 1, "Expected 1 ITOC entry" );
            itoc_filesl = query_utf_4byte(itoc_crypt_infile, itoc_offset+0x10, 0, "FilesL");
            itoc_filesh = query_utf_4byte(itoc_crypt_infile, itoc_offset+0x10, 0, "FilesH");
            CHECK_ERROR( itoc_filesl + itoc_filesh != CpkHeader_count, "CpkHeader file count and ITOC file counts do not match" );

            struct offset_size_pair offset_size;

            offset_size = query_utf_data(itoc_crypt_infile, itoc_offset+0x10, 0, "DataL");
            datal_offset = offset_size.offset + itoc_offset+0x10+8+result.data_offset;
            offset_size = query_utf_data(itoc_crypt_infile, itoc_offset+0x10, 0, "DataH");
            datah_offset = offset_size.offset + itoc_offset+0x10+8+result.data_offset;

            result = query_utf_nofail(itoc_crypt_infile, datal_offset, NULL);
            CHECK_ERROR( result.rows != itoc_filesl, "FilesL count does not match DataL rows");

            result = query_utf_nofail(itoc_crypt_infile, datah_offset, NULL);
            CHECK_ERROR( result.rows != itoc_filesh, "FilesH count does not match DataH rows");
        }

        /* extract files */
        long file_offset = content_offset;
        for (int i = 0, datal_i = 0, datah_i = 0; i < CpkHeader_count; i++)
        {
            /* see if we're going to DataL or DataH */
            uint16_t next_l_id = CpkHeader_count, next_h_id = CpkHeader_count;
            if (datal_i < itoc_filesl)
            {
                next_l_id = query_utf_2byte(itoc_crypt_infile, datal_offset,
                    datal_i, "ID");
            }
            if (datah_i < itoc_filesh)
            {
                next_h_id = query_utf_2byte(itoc_crypt_infile, datah_offset,
                    datah_i, "ID");
            }

            /* get file and extract size */
            long file_size, extract_size;
            if (next_l_id == i && next_h_id != i)
            {
                /* L is 2 byte sizes */
                file_size = query_utf_2byte(itoc_crypt_infile, datal_offset,
                    datal_i, "FileSize");
                extract_size = query_utf_2byte(itoc_crypt_infile, datal_offset,
                    datal_i, "ExtractSize");
                datal_i ++;
            }
            else if (next_h_id == i && next_l_id != i)
            {
                file_size = query_utf_4byte(itoc_crypt_infile, datah_offset,
                    datah_i, "FileSize");
                extract_size = query_utf_4byte(itoc_crypt_infile, datah_offset,
                    datah_i, "ExtractSize");
                datah_i ++;
            }
            else
            {
                CHECK_ERROR(next_l_id == i && next_h_id == i ,
                    "both DataL and DataH have the same ID");
                CHECK_ERROR(next_l_id != i && next_h_id != i ,
                    "neither DataL nor DataH have the next ID");
                CHECK_ERROR(1, "oops");
            }

            char *file_name = number_name("", ".bin", i, CpkHeader_count);
            printf("%s 0x%lx %ld\n",
                    file_name, (unsigned long)file_offset, file_size);
            FILE *outfile = open_file_in_directory(base_name, NULL, '/', file_name, "w+b");
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

            if (align != 0)
            {
                file_offset += (file_size + align-1)/align*align;
            }
            else
            {
                file_offset += file_size;
            }

            free(file_name);
        }

        close_reader(itoc_crypt_infile);
    }
    else
    {
        CHECK_ERROR(1, "no way to find files");
    }

#if 0
    else
    {
        /* even without a TOC we may be able to use a set of CRILAYLA headers to extract */
        const uint64_t CRILAYLA_sig = UINT64_C(0x4352494C41594C41);
        long file_offset = content_offset;
        int file_count = 0;
        long skipped_bytes = 0;
        int skip_count = 0;

        printf("no TOC found, faking it with ITOC\n");

        while (file_offset+8 <= content_offset + content_size &&
               file_count < CpkHeader_count)
        {
            if ( !(get_64_be_seek(file_offset+0x00, infile) == CRILAYLA_sig))
            {
                file_offset ++;
                skipped_bytes ++;
                continue;
            }

            if (skipped_bytes > 0)
            {
                char * skip_name = number_name("skipped_", ".bin", skip_count, CpkHeader_count);
                long skip_offset = file_offset-skipped_bytes;
                printf("%s 0x%lx\n", skip_name, skipped_bytes);
                reader_t *data_crypt_infile = infile;
                //data_crypt_infile = open_reader_crypt(infile, skip_offset, 0x5f, 0x15);
                FILE *skip_outfile = open_file_in_directory(base_name, NULL, '/', skip_name, "w+b");
                CHECK_ERRNO(!skip_outfile, "fopen");

                dump(data_crypt_infile, skip_outfile, skip_offset, skipped_bytes);

                CHECK_ERRNO(fclose(skip_outfile) != 0, "fclose");
                free(skip_name);
                skip_count ++;
                skipped_bytes = 0;
            }

            const long file_size = get_32_le_seek(file_offset+0x0C, infile) + 0x10 + 0x100;

            char * name = number_name("", ".bin", file_count, CpkHeader_count);

            printf("%s 0x%lx %ld\n",
                    name, (unsigned long)file_offset, file_size);
            FILE *outfile = open_file_in_directory(base_name, NULL, '/', name, "w+b");
            CHECK_ERRNO(!outfile, "fopen");

            long uncompressed_size =
                uncompress(infile, file_offset, file_size, outfile);
            printf("   uncompressed to %ld\n", uncompressed_size);

            /* done! */
            CHECK_ERRNO(fclose(outfile) != 0, "fclose");
            free(name);
            file_offset += file_size;
            file_count ++;
        }

        printf("found %d/%ld files\n", file_count, CpkHeader_count);
        printf("used 0x%lx/0x%lx bytes\n", file_offset - content_offset, content_size);
    }
#endif

    if (toc_string_table)
    {
        free_utf_string_table(toc_string_table);
        toc_string_table = NULL;
    }
}
