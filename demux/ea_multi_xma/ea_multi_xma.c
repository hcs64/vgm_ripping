#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

uint32_t read_32_be(unsigned char *b)
{
    uint32_t t = 0;
    for (int i = 0; i < 4; i++)
    {
        t *= 0x100;
        t += b[i];
    }

    return t;
}

/* return 1 if this is the last block, 0 otherwise */
int get_block_header(FILE *infile, long offset, uint32_t *block_size_p, uint32_t *block_samples_p, int *skip_p)
{
    unsigned char head_buf[4];
    int rc = 0;
    int skip = 0;

    if (block_size_p)
    {
        *block_size_p = 0;
    }
    if (block_samples_p)
    {
        *block_samples_p = 0;
    }

    if ( fseek(infile, offset, SEEK_SET) )
    {
        fprintf(stderr, "seek to 0x%lx to read block header failed\n", (unsigned long)offset);
        exit(EXIT_FAILURE);
    }

    if ( fread(head_buf, 1, 4, infile) != 4 )
    {
        fprintf(stderr, "read of block header at 0x%lx failed\n", (unsigned long)offset);
        exit(EXIT_FAILURE);
    }

    if ((head_buf[0] & 0x80) == 0x80)
    {
        head_buf[0] &= ~0x80;
        rc = 1;
    }

    if ((head_buf[0] & 0x40) == 0x40)
    {
        // ??
        head_buf[0] &= ~0x40;
    }

    if ((head_buf[0] & 0x08) == 0x08)
    {
        head_buf[0] &= ~0x08;
        skip = 1;
    }

    if ((head_buf[0] & 0x04) == 0x04)
    {
        // ??
        head_buf[0] &= ~0x04;
    }

    if ((head_buf[0] & 0x01) == 0x01)
    {
        head_buf[0] &= ~0x01;
        skip = 1;
        rc = 1;
    }

    if (0 != head_buf[0])
    {
        fprintf(stderr, "unknown flags at 0x%lx\n", (unsigned long)offset);
        exit(EXIT_FAILURE);
    }

    uint32_t block_size = read_32_be(&head_buf[0]);
    if (block_size < 4)
    {
        fprintf(stderr, "block too small at 0x%lx\n", (unsigned long)offset);
        exit(EXIT_FAILURE);
    }

    if (block_size_p)
    {
        *block_size_p = block_size;
    }
    if (!skip && block_samples_p)
    {
        if ( fread(head_buf, 1, 4, infile) != 4 )
        {
            fprintf(stderr, "failed reading sample count at 0x%lx\n", (unsigned long)offset);
            exit(EXIT_FAILURE);
        }
        *block_samples_p = read_32_be(&head_buf[0]);
    }
    if (skip_p)
    {
        *skip_p = skip;
    }

    return rc;
}

/* return subblock size (including header) in bytes */
uint32_t get_subblock_header(FILE *infile, long offset)
{
    unsigned char subhead_buf[4];
    uint32_t pseudo_size;

    if ( fseek(infile, offset, SEEK_SET) )
    {
        fprintf(stderr, "seek to 0x%lx to read subblock header failed\n", (unsigned long)offset);
        exit(EXIT_FAILURE);
    }
    if ( fread(subhead_buf, 1, 4, infile) != 4)
    {
        fprintf(stderr, "read of subblock header at 0x%lx failed\n", (unsigned long)offset);
        exit(EXIT_FAILURE);
    }

    pseudo_size = read_32_be(&subhead_buf[0]);

    return (pseudo_size / 4);   /* this value has rounding built in */
}

void usage(void)
{
    printf("ea_multi_xma 0.2\n");
    printf("usage:\n");
    printf("ea_multi_xma infile [-o 0xOffset]\n");
}

int main(int argc, char **argv)
{
    enum {XMA_FRAME_SIZE=0x800};
    unsigned char buf[XMA_FRAME_SIZE];

    FILE *infile;

    long start_offset = 0;

    if (argc != 2)
    {
        if (argc != 4 || strcmp(argv[2],"-o"))
        {
            usage();
            exit(EXIT_FAILURE);
        }
        else
        {
            /* -o option (start offset) */
            char *endp;
            start_offset = strtol(argv[3], &endp, 16);
            if (argv[3][0] == '\0' || endp[0] != '\0' || start_offset < 0)
            {
                usage();
                exit(EXIT_FAILURE);
            }
        }
    }
    
    infile = fopen(argv[1], "rb");
    if (!infile)
    {
        exit(EXIT_FAILURE);
    }

    /* detect stream count */

    unsigned int stream_count = 0;

    {
        long offset = start_offset;
        long true_start_offset;

        uint32_t block_size;
        int skip;

        do
        {
            true_start_offset = offset;
            get_block_header(infile, offset, &block_size, NULL, &skip);
            offset += block_size;
        }
        while (skip);

        for (offset = true_start_offset + 8; offset < true_start_offset + block_size; stream_count ++)
        {
            offset += get_subblock_header(infile, offset);
        }

        if (offset != true_start_offset + block_size || 0 == stream_count)
        {
            fprintf(stderr,"doesn't look like an EA multi XMA stream\n");
            exit(EXIT_FAILURE);
        }

        printf("%u stream%s\n", stream_count, (stream_count==1?"":"s"));

    }

    /* set up output files array */
    FILE **outfiles;
    outfiles = malloc(sizeof(FILE*) * stream_count);
    if (!outfiles)
    {
        exit(EXIT_FAILURE);
    }
    for (unsigned int i =0 ; i < stream_count; i ++)
    {
        outfiles[i] = NULL;
    }

    /* open output files */
    {
        size_t numlen = ceil(log10(stream_count+1));
        /* "name_stream#\0" */
        size_t namelen = strlen(argv[1]) + 1 + 6 + numlen + 1;
        char *namebuf = malloc(namelen);
        if (!namebuf)
        {
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < stream_count; i++)
        {
            snprintf(namebuf, namelen, "%s_stream%0*u", argv[1],
                    (int)numlen, (unsigned int)(i+1));
            outfiles[i] = fopen(namebuf, "wb");
            if (!outfiles[i])
            {
                exit(EXIT_FAILURE);
            }
            printf("%*u: %s\n", (int)numlen, (unsigned int)(i+1), namebuf);
        }
        free(namebuf);
    }

    /* rip! */
    long sample_count = 0;

    {
        int done = 0;
        long offset = start_offset;

        while (!done)
        {
            long block_start_offset = offset;
            uint32_t block_size, block_samples;
            int skip;
            done = get_block_header(infile, offset, &block_size, &block_samples, &skip);

            if (skip)
            {
                offset += block_size;
                continue;
            }

            offset += 8;

            for (unsigned int substream = 0; substream < stream_count; substream ++)
            {
                uint32_t subblock_size;

                subblock_size = get_subblock_header(infile, offset);

                offset += subblock_size;
                subblock_size -= 4;

                while (subblock_size >= XMA_FRAME_SIZE)
                {
                    if (fread(buf, 1, XMA_FRAME_SIZE, infile) != XMA_FRAME_SIZE) exit(EXIT_FAILURE);
                    if (fwrite(buf, 1, XMA_FRAME_SIZE, outfiles[substream]) != XMA_FRAME_SIZE) exit(EXIT_FAILURE);
                    subblock_size -= XMA_FRAME_SIZE;
                }

                if (subblock_size > 0)
                {
                    memset(buf,0xff,sizeof(buf));
                    //memset(buf,0,sizeof(buf));
                    if (fread(buf, 1, subblock_size, infile) != subblock_size) exit(EXIT_FAILURE);
                    if (fwrite(buf, 1, XMA_FRAME_SIZE, outfiles[substream]) != XMA_FRAME_SIZE) exit(EXIT_FAILURE);
                }
            }

            if (!(
                (block_start_offset + block_size == offset) ||  /* expected length */
                (done && block_start_offset + block_size > offset) )) /* less than expected, but padded */
            {
                printf("0x%lx != 0x%lx\n",(unsigned long)(block_start_offset + block_size),(unsigned long)offset);
                exit(EXIT_FAILURE);
            }

            sample_count += block_samples;
        }
    }

    for (unsigned int i = 0; i < stream_count; i++)
    {
        if ( fclose(outfiles[i]) )
        {
            exit(EXIT_FAILURE);
        }
    }
    free(outfiles);

    printf("%ld samples\n", sample_count);

    printf("Done!\n");

    return 0;
}
