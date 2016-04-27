#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

uint32_t le32(char *c)
{
    uint32_t v = 0;
    for (unsigned int i = 0 ; i < 4; i ++)
    {
        v *= 0x100;
        v += (unsigned char)c[3-i];
    }

    return v;
}

char dump_buf[BUFSIZ];

int main(int argc, char **argv)
{
    int rc = 1;

    char buf[16];
    FILE *idxfile = NULL, *binfile = NULL;

    if (3 != argc)
    {
        fprintf(stderr,
                "EZPie 0.0 - extract from IDX/BIN pairs (EZT/EZP)\n"
                "example usage: EZPie F_BGM.IDX F_BGM.BIN\n");
        goto fail;
    }

    idxfile = fopen(argv[1], "rb");
    if (!idxfile)
    {
        fprintf(stderr, "couldn't open %s: ",argv[1]);
        perror("fopen of IDX");
        goto fail;
    }

    binfile = fopen(argv[2], "rb");
    if (!binfile)
    {
        fprintf(stderr, "couldn't open %s: ",argv[2]);
        perror("fopen of BIN");
        goto fail;
    }

    char *name_table = NULL;

    /* read header of IDX */
    if (16 != fread(buf, 1, 16, idxfile))
    {
        if (0 != errno)
        {
            perror("fread of IDX header");
        }
        else
        {
            fprintf(stderr, "fread of IDX header failed\n");
        }
        goto fail;
    }

    if (memcmp("EZT", &buf[0], 4))
    {
        fprintf(stderr, "bad IDX header\n");
        goto fail;
    }

    /* maybe these ids are related to file size */
    const uint32_t file_id  = le32(&buf[4]);
    const uint32_t file_id2 = le32(&buf[8]);

    const uint32_t file_count = le32(&buf[12]);

    /* get BIN size */
    if (0 != fseek(binfile,0,SEEK_END))
    {
        perror("fseek for BIN size");
        goto fail;
    }
    const long bin_size = ftell(binfile);
    if (-1 == bin_size)
    {
        perror("ftell");
        goto fail;
    }

    if (0 != fseek(binfile,0,SEEK_SET))
    {
        perror("fseek to start");
        goto fail;
    }

    /* read header of BIN */
    if (16 != fread(buf, 1, 16, binfile))
    {
        if (0 != errno)
        {
            perror("fread of BIN header");
        }
        else
        {
            fprintf(stderr,"fread of BIN header failed\n");
        }
        goto fail;
    }

    if (memcmp("EZP", &buf[0], 4))
    {
        fprintf(stderr, "bad BIN header\n");
        goto fail;
    }

    if (le32(&buf[4]) != file_id ||
        le32(&buf[8]) != file_id2)
    {
        fprintf(stderr, "header mismatch\n");
        goto fail;
    }

    const uint32_t name_table_offset = le32(&buf[12]);
    const long names_size = bin_size - name_table_offset;
    printf("name_table_offset = %"PRIx32"\n", name_table_offset);
    printf("bin_size = %lx\n", (unsigned long)bin_size);

    /* get the names */
    name_table = malloc(names_size + 1);
    if (!name_table)
    {
        perror("name table malloc");
        goto fail;
    }
    memset(name_table, 0, names_size + 1);

    if (0 != fseek(binfile,name_table_offset,SEEK_SET))
    {
        perror("fseek to names");
        goto fail;
    }
    if (names_size != fread(name_table, 1, names_size, binfile))
    {
        if (0 != errno)
        {
            perror("fread of names");
        }
        else
        {
            fprintf(stderr, "fread of names failed\n");
        }
        goto fail;
    }

    /* process files */
    printf("idx    offset     size name\n"
           "-------------------------------\n");
         /*"012: 01234567 01234567 name"*/
    for (unsigned int i = 0; i < file_count; i++)
    {
        if (12 != fread(buf, 1, 12, idxfile))
        {
            if (0 != errno)
            {
                perror("fread of IDX entry");
            }
            else
            {
                fprintf(stderr, "fread of IDX entry failed\n");
            }
            goto fail;
        }

        const uint32_t file_offset = le32(&buf[0]);
        const uint32_t file_size = le32(&buf[4]);
        const uint32_t name_offset = le32(&buf[8]);

        printf("%3u: %8"PRIx32" %8"PRIx32" %s\n",
            i, file_offset, file_size, &name_table[name_offset]);
        fflush(stdout);

        if (0 != file_size)
        {
            /* dump file */
            FILE *outfile = NULL;
            /* open output */
            outfile = fopen(&name_table[name_offset], "wb");
            if (!outfile)
            {
                perror("fopen of output");
                goto fail;
            }

            uint32_t bytes_dumped;
            if (0 != fseek(binfile, file_offset, SEEK_SET))
            {
                perror("fseek to file");
                goto fail;
            }

            for (bytes_dumped = 0; bytes_dumped+sizeof(dump_buf) < file_size;
                 bytes_dumped += sizeof(dump_buf))
            {
                if (sizeof(dump_buf) !=
                    fread(dump_buf, 1, sizeof(dump_buf), binfile))
                {
                    if (0 != errno)
                    {
                        perror("fread for dumping");
                    }
                    else
                    {
                        fprintf(stderr, "fread for dumping failed");
                    }
                    fclose(outfile);
                    goto fail;
                }

                if (sizeof(dump_buf) !=
                    fwrite(dump_buf, 1, sizeof(dump_buf), outfile))
                {
                    perror("fwrite for dumping");
                    fclose(outfile);
                    goto fail;
                }
            }

            if (file_size-bytes_dumped !=
                fread(dump_buf, 1, file_size-bytes_dumped, binfile))
            {
                if (0 != errno)
                {
                    perror("fread for dumping");
                }
                else
                {
                    fprintf(stderr, "fread for dumping failed");
                }
                fclose(outfile);
                goto fail;
            }
            if (file_size-bytes_dumped !=
                fwrite(dump_buf, 1, file_size-bytes_dumped, outfile))
            {
                perror("fwrite for dumping");
                goto fail;
            }

            if (0 != fclose(outfile))
            {
                perror("fclose of output");
                goto fail;
            }
        }
    }

    rc = 0;

    printf("done!\n");

fail:

    if (name_table)
    {
        free(name_table);
    }
    if (binfile)
    {
        fclose(binfile);
    }
    if (idxfile)
    {
        fclose(idxfile);
    }

    return rc;
}
