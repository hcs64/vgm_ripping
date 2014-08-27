#include <stdio.h>

#include "utf_tab.h"
#include "cpk_uncompress.h"
#include "util.h"
#include "error_stuff.h"

void analyze_CRID(FILE *infile, const char *infile_name, long file_length, int verbosity);

void usage(void)
{
    fflush(stdout);
    fprintf(stderr,
        "Incorrect program usage\n\nusage: usm_deinterleave file [-v|-vv|-q]\n\n"
        "-v  : verbose (header info)\n"
        "-vv : more verbose (header and block info)\n"
        "-q  : quiet (only output on error)\n");
}

enum
{
    verbose_quiet,
    verbose_normal,
    verbose_headers,
    verbose_blocks
};

int main(int argc, char **argv)
{
    int verbosity = verbose_normal;
    int args_ok = 0;

    if (argc == 2)
    {
    args_ok = 1;
    }
    else if (argc == 3)
    {
        if (!strcmp(argv[2],"-v"))
        {
            verbosity = verbose_headers;
            args_ok = 1;
        }
        else if (!strcmp(argv[2],"-vv"))
        {
            verbosity = verbose_blocks;
            args_ok = 1;
        }
        else if (!strcmp(argv[2],"-q"))
        {
            verbosity = verbose_quiet;
            args_ok = 1;
        }
    }

    if (verbosity >= verbose_normal)
    {
        printf("usm_deinterleave " VERSION "\n\n");
    }

    if (!args_ok)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    analyze_CRID(infile, argv[1], file_length, verbosity);

    exit(EXIT_SUCCESS);
}

void analyze_CRID(FILE *infile, const char *infile_name, long file_length, int verbosity)
{
    long stream_count = 0;
    struct stream_info *streams = NULL;
    char *CRIUSF_string_table = NULL;

    FILE **outfiles = NULL;
    char **outfile_names = NULL;

    enum
    {
        CRID_stmid = 0x43524944,  /* CRID */
        SFV_stmid = 0x40534656,   /* @SFV */
        SFA_stmid = 0x40534641    /* @SFA */
    };

    struct stream_info
    {
        const char *filename;
        uint32_t filesize;
        uint32_t datasize;
        uint32_t stmid;
        uint16_t chno;
        uint16_t minchk;
        uint32_t minbuf;
        uint32_t avbps;

        long bytes_read;
        long payload_bytes;
        unsigned int alive;
    };


    int live_streams = 0;
    int streams_setup = 0;

    /* dispense justice! */
    do
    {
        uint32_t stmid = get_32_be(infile);
        uint32_t block_size = get_32_be(infile);
        uint32_t block_type;
        uint16_t header_size, footer_size;
        size_t payload_bytes;
        int stream_idx = 0;

        if (!streams_setup)
        {
            CHECK_ERROR (CRID_stmid != stmid, "file should start with CRID");
        }
        else /* streams already set up */
        {
            CHECK_ERROR (0 == stream_count, "0 stream count should be impossible");
            /* find the stream */
            for (stream_idx=1; stream_idx < stream_count; stream_idx++)
            {
                if (stmid == streams[stream_idx].stmid)
                {
                    break;
                }
            }
            CHECK_ERROR (stream_idx == stream_count, "unknown stmid");

            CHECK_ERROR (!streams[stream_idx].alive, "stream was supposed to be ended");

            streams[stream_idx].bytes_read += block_size;
        }

        if (verbosity >= verbose_blocks)
        {
            printf("%08lx %d block_size: %08"PRIx32" ", (unsigned long)ftell(infile)-8, stream_idx, block_size);
        }

        /* block control */
        {
            header_size = get_16_be(infile);
            footer_size = get_16_be(infile);
            if (verbosity >= verbose_blocks)
            {
                printf("%04"PRIx16" %04"PRIx16"\n", header_size, footer_size);
            }

            CHECK_ERROR( 0x18 != header_size, "expected header size 0x18" );
            payload_bytes = block_size - header_size - footer_size;
        }

        /* block typs */
        block_type = get_32_be(infile);
        if (verbosity >= verbose_blocks)
        {
            printf("type %08"PRIx32"\n", block_type);
        }

        /* check the rest of the block header */
        {
            uint32_t byte1,byte2,byte3,byte4;

            byte1 = get_32_be(infile); /* granule (1/100 of a frame) */
            byte2 = get_32_be(infile); /* samples (based on whole block size at avg bitrate) */
            byte3 = get_32_be(infile);
            byte4 = get_32_be(infile);

            if (verbosity >= verbose_blocks)
            {
                printf("%08"PRIx32" %08"PRIx32" %08"PRIx32" %08"PRIx32"\n\n",
                        byte1,byte2,byte3,byte4);
            }

            CHECK_ERROR(
                    (//(0    != byte1) ||
                     //(0x1E != byte2) ||
                     (0    != byte3) ||
                     (0    != byte4)), "block unknown bytes mismatch");
        }

        if (!streams_setup)
        {
            /* handle first block, which describes the subsequent streams */

            long CRIUSF_offset = ftell(infile);
            CHECK_ERROR (-1 == CRIUSF_offset, "ftell");

            /* seems like it ought to be type 2, but it's type 1 */
            CHECK_ERROR (1 != block_type, "CRID should be type 1");

            if (verbosity >= verbose_headers)
            {
                analyze_utf(infile, CRIUSF_offset, 0, 1, NULL);
            }

            /* check CRIUSF stream list */
            {
                struct utf_query_result result = query_utf_nofail(infile, CRIUSF_offset, NULL);
                long CRIUSF_data_offset;

                CHECK_ERROR (result.rows < 1, "expected at least one row in CRIUSF");
                stream_count = result.rows;
                CRIUSF_data_offset = CRIUSF_offset + result.data_offset;

                CRIUSF_string_table = load_utf_string_table(infile, CRIUSF_offset);

                /* check that we're actually looking at a CRIUSF table */
                CHECK_ERROR (strcmp(&CRIUSF_string_table[result.name_offset],
                            "CRIUSF_DIR_STREAM"), "expected CRIUSF_DIR_STREAM");

            }

            /* check streams */
            {
                int i, j;

                streams = malloc(sizeof(struct stream_info)*stream_count);
                CHECK_ERRNO (!streams, "malloc");
                memset(streams, 0, sizeof(struct stream_info)*stream_count);

                for (i = 0; i < stream_count; i ++)
                {
                    struct stream_info * const s = &streams[i];
                    s->filename = query_utf_string(infile, CRIUSF_offset, i, "filename", CRIUSF_string_table);
                    s->filesize = query_utf_4byte(infile, CRIUSF_offset, i, "filesize");
                    s->datasize = query_utf_4byte(infile, CRIUSF_offset, i, "datasize");
                    s->stmid    = query_utf_4byte(infile, CRIUSF_offset, i, "stmid");
                    s->chno     = query_utf_2byte(infile, CRIUSF_offset, i, "chno");
                    s->minchk   = query_utf_2byte(infile, CRIUSF_offset, i, "minchk");
                    s->minbuf   = query_utf_4byte(infile, CRIUSF_offset, i, "minbuf");
                    s->avbps    = query_utf_4byte(infile, CRIUSF_offset, i, "avbps");

                    if (0 == i)
                    {
                        CHECK_ERROR ((0 != s->stmid), "expected 0 stmid for stream 0");

                        /* does this indicate that the normal type is signed, and
                           type 2 is unsigned? */
                        CHECK_ERROR ((65535 != s->chno), "expected -1 chno");
                    }
                    else
                    {
                        switch (s->stmid)
                        {
                            case SFV_stmid:
                                if (verbosity >= verbose_normal)
                                {
                                    printf("Stream %d: Video\n", i);
                                }
                                break;
                            case SFA_stmid:
                                if (verbosity >= verbose_normal)
                                {
                                    printf("Stream %d: Audio\n", i);
                                }
                                break;
                            default:
                                CHECK_ERROR (1, "unknown stmid");
                        }

                        /* possibility of multiple channels? probably sees use with audio */
                        CHECK_ERROR ((0 != s->chno), "expected 0 chno");
                    }

                    CHECK_ERROR ((0 != s->datasize), "expected 0 datasize");

                    /* check for redundant stmid */
                    for (j = 0; j < i; j++)
                    {
                        CHECK_ERROR (s->stmid == streams[j].stmid, "duplicate stmid");
                    }
                }
            }

            /* open output files */
            {
                int i;
                outfiles = malloc(sizeof(FILE*)*stream_count);
                CHECK_ERRNO (!outfiles, "malloc");
                outfile_names = malloc(sizeof(char*)*stream_count);
                CHECK_ERRNO (!outfile_names, "malloc");

                for (i = 0; i < stream_count; i++)
                {
                    if (0 == i)
                    {
                        outfiles[i] = NULL;
                        outfile_names[i] = NULL;
                    }
                    else
                    {
                        size_t outfile_name_length = strlen(infile_name) + 1 + 3 + 4 + 1;
                        char *name = malloc(outfile_name_length);
                        CHECK_ERRNO (!name, "malloc");

                        switch (streams[i].stmid)
                        {
                            case SFV_stmid:
                                snprintf(name,outfile_name_length,"%s_%d.mpg", infile_name, i);
                                break;
                            case SFA_stmid:
                                snprintf(name,outfile_name_length,"%s_%d.adx", infile_name, i);
                                break;
                        }
                        outfile_names[i] = name;
                        outfiles[i] = fopen(name, "wb");

                        CHECK_ERRNO(!outfiles[i], "fopen");
                    }
                }
            }

            /* initialize streams */
            {
                int i;
                streams[0].alive = 0;
                for (i=1; i < stream_count; i++)
                {
                    streams[i].alive = 1;
                    live_streams ++;
                    streams[i].bytes_read = 0;
                    streams[i].payload_bytes = 0;
                }
            }

            streams_setup = 1;

            /* seek to footer for check below */
            fseek(infile, CRIUSF_offset+payload_bytes, SEEK_SET);
        }
        else    /* stream setup already complete */
        {
            /* handle different block types */
            switch (block_type)
            {

                case 0: /* data */
                    dump_from_here(infile, outfiles[stream_idx], payload_bytes);
                    streams[stream_idx].payload_bytes += payload_bytes;
                    break;
                case 1: /* header */
                case 3: /* metadata */
                    /* skip */
                    {
                        long current_offset = ftell(infile);
                        CHECK_ERROR (-1 == current_offset, "ftell");
                        if (
                            (block_type == 1 && verbosity >= verbose_headers) ||
                            (block_type == 3 && verbosity >= verbose_blocks))
                        {
                            analyze_utf(infile, current_offset, 0, 1, NULL);
                        }
                        fseek(infile, current_offset+payload_bytes, SEEK_SET);
                    }
                    break;
                case 2: /* stream metadata */
                    {
                        char *metadata = malloc(payload_bytes);
                        CHECK_ERROR (!metadata, "malloc");

                        get_bytes(infile, (unsigned char *)metadata, payload_bytes);

                        if (!strncmp(metadata, "#HEADER END     ===============", payload_bytes))
                        {
                        }
                        else
                        if (!strncmp(metadata, "#METADATA END   ===============", payload_bytes))
                        {
                        }
                        else
                        if (!strncmp(metadata, "#CONTENTS END   ===============", payload_bytes))
                        {
                            streams[stream_idx].alive = 0;
                            live_streams --;
                        }
                        else
                        {
                            CHECK_ERROR (1, "unknown stream metadata");
                        }
                    }
                    break;
                default:
                    CHECK_ERROR (1, "unknown block type");
            }
        }

        /* check footer (0 padding) */
        {
            int i;
            for (i = 0; i < footer_size; i++)
            {
                CHECK_ERROR (0 != get_byte(infile), "nonzero padding");
            }
        }
    }
    while (live_streams > 0);

    CHECK_ERROR (!streams_setup, "no CRID found");

    if (ftell(infile) != file_length)
    {
        printf("Warning: read only 0x%lx bytes of 0x%lx byte file\n",
            (unsigned long)ftell(infile), (unsigned long)file_length);
    }

    /* cleanup */
    if (outfiles)
    {
        int i;
        for (i=0; i < stream_count; i++)
        {
            if (outfiles[i])
            {
                if (verbosity >= verbose_normal)
                {
                    printf("%d: %s: read %ld bytes, %ld bytes payload\n",
                            i, outfile_names[i], streams[i].bytes_read, streams[i].payload_bytes);
                }
                int rc = fclose(outfiles[i]);
                CHECK_ERRNO (EOF == rc, "fclose");
            }
        }

        free(outfiles);
    }

    if (outfile_names)
    {
        int i;
        for (i=0; i < stream_count; i++)
        {
            if (outfile_names[i])
            {
                free(outfile_names[i]);
            }
        }

        free(outfile_names);
    }

    if (streams)
    {
        free(streams);
        streams = NULL;
    }

    if (CRIUSF_string_table)
    {
        free_utf_string_table(CRIUSF_string_table);
        CRIUSF_string_table = NULL;
    }
}
