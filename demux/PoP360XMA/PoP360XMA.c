#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "error_stuff.h"

int main(int argc, char **argv)
{
    printf("Pop360XMA 0.1\n");
    CHECK_ERROR(argc != 2 && argc != 3, "usage: PoP360XMA infile [offset]");

    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen input");

    if (argc == 3)
    {
        CHECK_ERRNO(-1 == fseek(infile, read_long(argv[2]), SEEK_SET),"fseek");
    }

    unsigned char head_buf[0x40];
    CHECK_FILE( 0x40 != fread(head_buf, 1, 0x40, infile), infile,
            "fread header");

    uint32_t expected_streams = read_32_be(&head_buf[0x30]);

    uint32_t next_size;

    uint32_t *stream_total_size = NULL;
    unsigned int stream_count = 0;

    // first block, stream totals
    {
        bool more_streams = true;

        next_size = get_32_be(infile);

        /* stream total sizes block */
        while (more_streams)
        {
            uint32_t stream_size = get_32_be(infile);

            if (more_streams)
            {
                if ( 0 == stream_size )
                {
                    more_streams = false;
                }
                else
                {
                    //fprintf(stderr, "stream %d: %"PRIu32" bytes\n",
                    //        i, stream_size);

                    stream_count ++;

                    stream_total_size = realloc(stream_total_size,
                        sizeof(uint32_t)*stream_count);
                    CHECK_ERROR( !stream_total_size, "realloc" );

                    stream_total_size[stream_count-1] = stream_size;
                }
            }
        }

        for (unsigned int i = 1; i < 0x40/4; i++)
        {
            uint32_t pad = get_32_be(infile);
            CHECK_ERROR( 0 != pad, "expected 0 padding after..." );
        }
    }

    CHECK_ERROR( expected_streams != stream_count, "stream count mismatch" );

    CHECK_ERROR( stream_count > 999, "arbitrary limit of max 999 streams" );

    FILE **outfile = malloc(sizeof(FILE *) * stream_count);
    CHECK_ERROR( !outfile, "malloc" );

    uint32_t *stream_chunk_size = malloc(sizeof(uint32_t) * stream_count);
    CHECK_ERROR( !stream_chunk_size, "malloc" );

    for (unsigned int i = 0; i < stream_count; i++)
    {
        unsigned int len = strlen(argv[1]) + 1 + 3 + 1;
        char *dumpname = malloc(len);
        snprintf(dumpname, len, "%s_%03u", argv[1], i);

        outfile[i] = fopen(dumpname, "wb");
        CHECK_ERRNO(!outfile, "fopen output");

        printf("%d: %s %"PRIu32" bytes\n", i, dumpname, stream_total_size[i]);
    }

    // process payload chunks
    while (0 != next_size)
    {
        uint32_t magic = get_32_be(infile);
        uint32_t cur_size = next_size;
        next_size = get_32_be(infile);

        CHECK_ERROR( 3 != magic, "expected 0x03 chunk" );

        uint32_t all_stream_size = 0;
        for (unsigned int i = 0; i < stream_count; i++)
        {
            stream_chunk_size[i] = get_32_be(infile);
            all_stream_size += stream_chunk_size[i];
            //fprintf(stderr, "stream %d: %"PRIx32"\n", i, stream_size[i]);
        }

        if ( 8 + 4 * stream_count + all_stream_size != cur_size )
        {
            long cur_offset = ftell(infile);
            fprintf(stderr, "offset = %lx "
                    "size (from previous) = %"PRIx32"\n",
                    (unsigned long)cur_offset, cur_size);
            CHECK_ERROR( 1, "size doesn't match calculated" );
        }

        for (unsigned int i = 0; i < stream_count; i++)
        {
            dump_here(infile, outfile[i], stream_chunk_size[i]);
        }
    }

    // cleanup

    free(stream_chunk_size);

    for (unsigned int i = 0; i < stream_count; i++)
    {
        CHECK_ERROR (stream_total_size[i] != ftell(outfile[i]),
            "didn't read exactly whole stream");
        CHECK_ERRNO(EOF == fclose(outfile[i]), "fclose");
    }

    free(outfile);

    free(stream_total_size);

    printf("Done!\n");
}
