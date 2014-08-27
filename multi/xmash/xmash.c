#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "bitstream.h"
#include "guessfsb.h"
#include "fsbext.h"
#include "riffext.h"
#include "bnkext.h"
#include "xma_rebuild.h"

// XMAsh - decrypt, demux, and rebuild FSB XMA2
// also minimal RIFF and WWise bnk support
// cobbled together from:
// romchu, guessfsb, decfsb, fsb_mpeg, xma_parse, vgmstream

#define VERSION "0.8"
#define BIN_NAME "xmash"

const char *dir_name;

struct main_info {
    const char *file_name;
};

int try_key_callback(const uint8_t * d, long size, void *v);
int subfile_callback(const uint8_t * infile, long size, int streams, int *stream_channels, long samples, long srate, long block_size, long loop_start, long loop_end, const char *stream_name, void *v);

void usage(void);

int main(int argc, char **argv)
{
    long file_size;
    uint8_t *indata;

    int success = 0;

    struct main_info mi;

    if (argc != 2 && (argc != 4 || (argc == 4 && strcmp(argv[2], "-o"))))
    {
        usage();
    }

    char * infile_name = argv[1];

    if (argc == 4)
    {
        dir_name = argv[3];
    }
    else
    {
        dir_name = NULL;
    }

    mi.file_name = strip_path(infile_name);

    {
        FILE *infile;
        infile = fopen(infile_name, "rb");
        CHECK_ERRNO(!infile, "open input failed");
        indata = get_whole_file(infile, &file_size);
        fclose(infile);
    }

    printf("%s\n\n", infile_name);

    // try without decrypting
    if (0 == try_multistream_fsb(indata, file_size, subfile_callback, &mi))
    {
        printf("success without decryption!\n");
        success = 1;
    }
    // try RIFF
    else if (0 == try_xma_riff(indata, file_size, subfile_callback, &mi))
    {
        printf("success with RIFF!\n");
        success = 1;
    }
    else if (0 == try_wwbnk(indata, file_size, subfile_callback, &mi))
    {
        printf("success with WWise\n");
        success = 1;
    }
    // try via guessfsb
    else if (0 == guess_fsb_keys(indata, file_size, try_key_callback, &mi))
    {
        printf("success!\n");
        success = 1;
    }
    else
    {
        printf("failure.\n");
    }

    free(indata);

    if (success)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void usage()
{
    fprintf(stderr, "XMAsh " VERSION " - decrypt, demux, and rebuild FSB, Wwise .bnk, RIFF XMA2\n");
    fprintf(stderr, "usage:\n"
                    "  " BIN_NAME " input.fsb.xen [-o dir]\n"
                    "  " BIN_NAME " input.fsb [-o dir]\n"
                    "  " BIN_NAME " input.xma [-o dir]\n"
                    "  " BIN_NAME " input.bnk [-o dir]\n");
    exit(EXIT_FAILURE);
}

// called for each possibly valid key
int try_key_callback(const uint8_t * indata, long size, void *v)
{
    return try_multistream_fsb(indata, size, subfile_callback, v);
}

// called for each subfile in an FSB (or a RIFF body)
int subfile_callback(const uint8_t * infile, long size, int streams, int * stream_channels, long samples, long srate, long block_size, long loop_start, long loop_end, const char *stream_name, void *v)
{
    const struct main_info *mip = v;

    // build the name
    int name_base_len = strlen(mip->file_name) + 1 + strlen(stream_name);
    char *name_base = malloc(name_base_len+1);
    CHECK_ERRNO(!name_base, "malloc");
    sprintf(name_base, "%s_%s", mip->file_name, stream_name);


    if (streams == 0 && !stream_channels && samples == 0 && srate == 0)
    {
        FILE *outfile;
        char *strname = number_name(name_base, ".bin", 0, 1);
        printf("dumping %s\n", strname);

        // just dump
        if (dir_name)
        {
            outfile = open_file_in_directory(dir_name, NULL, DIRSEP, strname, "wb");
        }
        else
        {
            outfile = fopen(strname, "wb");
        }
        put_bytes(outfile, infile, size);
        fclose(outfile);
        free(strname);
        free(name_base);

        return 0;
    }

    // rebuild each stream
    for (int str = 0; str < streams; str ++)
    {
        long parsed_samples;
        int channels;
        long data_offset, data_size;
        char *strname = number_name(name_base, ".xma", str+1, streams);
        printf("%s\n", strname);

        FILE *outfile;
        
        if (dir_name)
        {
            outfile = open_file_in_directory(dir_name, NULL, DIRSEP, strname, "wb");
        }
        else
        {
            outfile = fopen(strname, "wb");
        }
        CHECK_ERRNO(!outfile, "fopen output");
        CHECK_ERRNO(
            -1 == fseek(outfile, xma_header_size, SEEK_SET), "fseek past header");

        data_offset = packet_size_bytes*str;
        data_size = size-data_offset;

        // rebuild!
        if (stream_channels)
        {
            channels = stream_channels[str];
        }
        else
        {
            channels = 2;
        }
        if (0 != build_XMA_from_XMA2(infile + data_offset,
                                     data_size,
                                     outfile, block_size, channels,
                                     &parsed_samples))
        {
            // encountered an error while parsing
            fclose(outfile);
            free(strname);
            free(name_base);
            free(stream_channels);
            return 1;
        }

        if (parsed_samples != samples)
        {
            printf("parsed samples = %ld, expected %ld\n", parsed_samples, samples);
            if (parsed_samples > samples)
            {
                printf("but that's more so we'll let it slide...\n");
            }
            else
            {
                fclose(outfile);
                free(strname);
                free(name_base);
                free(stream_channels);
                return 1;
            }
        }

        // write header
        long finish_offset = ftell(outfile);
        uint8_t *xma_head = make_xma_header(srate, finish_offset-xma_header_size, channels);
        CHECK_ERRNO(
            -1 == fseek(outfile, 0, SEEK_SET), "fseek to header");
        CHECK_ERRNO( 1 != fwrite(xma_head, xma_header_size, 1, outfile) , "fwrite header");

        CHECK_ERRNO(EOF == fclose(outfile), "fclose");
        free(xma_head);

        free(strname);

        // write loop
        if (loop_end > 0)
        {
            char *pos_name = number_name(name_base, ".pos", str+1, streams);
            FILE *posfile;
            
            if (dir_name)
            {
                posfile = open_file_in_directory(dir_name, NULL, DIRSEP, pos_name, "wb");
            }
            else
            {
                posfile = fopen(pos_name,"wb");
            }

            CHECK_ERRNO(!posfile, "fopen .pos");
            put_32_le(loop_start, posfile);
            put_32_le(loop_end, posfile);
            fclose(posfile);

            printf("wrote loop to %s\n", pos_name);

            free(pos_name);
        }
    }

    free(name_base);
    free(stream_channels);
    return 0;
}


