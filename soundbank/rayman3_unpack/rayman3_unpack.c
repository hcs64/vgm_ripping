#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

int main(int argc, char **argv)
{
    FILE *infile = NULL;
    FILE *outfile_L = NULL, *outfile_R = NULL;

    // check argument count
    if (2 != argc)
    {
        fprintf(stderr,"usage: rayman3_unpack rayman3.hst\n");
        goto fail;
    }

    // open input
    infile = fopen(argv[1], "rb");
    if (!infile)
    {
        fprintf(stderr, "failed opening %s: ", argv[1]);
        perror(NULL);
        goto fail;
    }

    // process each embedded file
    for (unsigned int file_count = 0; ; file_count ++)
    {
        // get headers
        enum {DSP_header_size = 0x60,
            frame_size = 8,
            frame_pair_size = 16
        };
        unsigned char header_L[DSP_header_size];
        unsigned char header_R[DSP_header_size];

        size_t first_read = fread(header_L, 1, DSP_header_size, infile);
        if (0 <= first_read && DSP_header_size > first_read && feof(infile))
        {
            // check partial read at end
            for (unsigned int i = 0; i < first_read; i++)
            {
                if (0 != header_L[i])
                {
                    fprintf(stderr, "expected zero padding at file end\n");
                    goto fail;
                }
            }

            // normal exit
            break;
        }

        if ( DSP_header_size != first_read ||
            DSP_header_size != fread(header_R, 1, DSP_header_size, infile))
        {
            fprintf(stderr, "fread of DSP headers failed: ");
            if (feof(infile))
            {
                fprintf(stderr, "unexpected end of file\n");
            }
            else
            {
                perror(NULL);
            }
            goto fail;
        }

        bool stereo = true;

        // basic consistency check
        if (memcmp(&header_L[0], &header_R[0], 0x1C))
        {
            stereo = false;
        }

        // open output files
        {
            char namebuf[4+2+1+3+1]; // 0123_L.dsp\0
            if (10000 < file_count)
            {
                fprintf(stderr, "too many files!?!\n");
                goto fail;
            }

            if (stereo)
            {
                snprintf(namebuf, sizeof(namebuf), "%04u_L.dsp", file_count);
            }
            else
            {
                snprintf(namebuf, sizeof(namebuf), "%04u.dsp", file_count);
            }

            outfile_L = fopen(namebuf, "wb");
            if (!outfile_L)
            {
                fprintf(stderr, "failed opening %s for output: ", namebuf);
                perror(NULL);
                goto fail;
            }

            if (stereo)
            {
                snprintf(namebuf, sizeof(namebuf), "%04u_R.dsp", file_count);
                outfile_R = fopen(namebuf, "wb");
                if (!outfile_R)
                {
                    fprintf(stderr, "failed opening %s for output: ", namebuf);
                    perror(NULL);
                    goto fail;
                }
            }
        }

        // dump headers
        if (DSP_header_size != fwrite(header_L, 1, DSP_header_size, outfile_L) ||
            (stereo && DSP_header_size != fwrite(header_R, 1, DSP_header_size, outfile_R)))
        {
            perror("fwrite of DSP header failed");
            goto fail;
        }

        // dump stream
        {
            uint32_t nibble_count = 0;
            for (unsigned int i = 0; i < 4; i++)
            {
                nibble_count *= 0x100U;
                nibble_count |= header_L[4+i];
            }

            unsigned int byte_count = nibble_count / 0x10 * 8;
            if (nibble_count % 0x10 > 2)
            {
                fprintf(stderr, "wasn't expecting to see you here!\n");
            }

            // status
            {
                long offset = ftell(infile);
                if (-1 != offset)
                {
                    printf("0x%08lx: %04d: %s %d bytes\n",
                            (unsigned long)offset - DSP_header_size*2,
                            file_count, (stereo?"stereo":"mono"), byte_count);
                }
            }

            if (!stereo)
            {
                // mono

                // dump "right channel header", which is really beginning of
                // mono data
                if (DSP_header_size != fwrite(&header_R[0], 1, DSP_header_size, outfile_L))
                {
                    perror("fwrite of mono start failed");
                    goto fail;
                }

                for (unsigned int i = DSP_header_size / 8; i < byte_count / 8; i ++)
                {
                    unsigned char buf[frame_size];
                    if (frame_size != fread(&buf[0], 1, frame_size, infile))
                    {
                        fprintf(stderr, "fread of frame failed: ");
                        if (feof(infile))
                        {
                            fprintf(stderr, "unexpected end of file\n");
                        }
                        else
                        {
                            perror(NULL);
                        }
                        goto fail;
                    }

                    if (frame_size != fwrite(&buf[0], 1, frame_size, outfile_L))
                    {
                        perror("fwrite of frame failed");
                        goto fail;
                    }
                }
            }
            else
            {
                // stereo

                for (unsigned int i = 0; i < byte_count / 8; i ++)
                {
                    unsigned char buf[frame_pair_size];
                    if (frame_pair_size != fread(&buf[0], 1, frame_pair_size, infile))
                    {
                        fprintf(stderr, "fread of frames failed: ");
                        if (feof(infile))
                        {
                            fprintf(stderr, "unexpected end of file\n");
                        }
                        else
                        {
                            perror(NULL);
                        }
                        goto fail;
                    }

                    if (frame_size != fwrite(&buf[0], 1, frame_size, outfile_L))
                    {
                        perror("fwrite of left frame failed");
                        goto fail;
                    }
                    if (frame_size != fwrite(&buf[8], 1, frame_size, outfile_R))
                    {
                        perror("fwrite of right frame failed");
                        goto fail;
                    }
                }
            }
        }

        // close outputs
        if (EOF == fclose(outfile_L))
        {
            perror("fclose");
            outfile_L = NULL;
            goto fail;
        }
        outfile_L = NULL;

        if (stereo)
        {
            if (EOF == fclose(outfile_R))
            {
                perror("fclose");
                outfile_R = NULL;
                goto fail;
            }
            outfile_R = NULL;
        }
    }

    fclose(infile);

    printf("Done!\n");

    return 0;

fail:
    if (infile)
    {
        fclose(infile);
    }
    if (outfile_L)
    {
        fclose(outfile_L);
    }
    if (outfile_R)
    {
        fclose(outfile_R);
    }

    fprintf(stderr, "failed\n");

    return 1;
}
