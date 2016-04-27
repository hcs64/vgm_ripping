#include <stdlib.h>
#include <stdio.h>
#include "error_stuff.h"
#include "util.h"

void reinterleave_ms_ima(FILE *infile, long offset, size_t size, unsigned int channels, unsigned int block_size)
{
    unsigned char *buf = malloc(block_size);
    unsigned char *buf2 = malloc(block_size);   // deinterleave target
    const unsigned int bytes_per_channel = block_size / channels;
    CHECK_ERROR( (size % block_size), "block size doesn't go evenly into data size" );
    CHECK_ERROR( (block_size % channels), "block size doesn't divide evenly by channels" );
    CHECK_ERROR( (bytes_per_channel % 4), "expected multiple of 4 bytes per channel" );

    CHECK_ERRNO( !buf, "malloc" );
    CHECK_ERRNO( !buf2, "malloc" );

    while (size > 0)
    {
        CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

        size_t bytes_read = fread(buf, 1, block_size, infile);
        CHECK_FILE(bytes_read != block_size, infile, "fread");

        CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

        for (unsigned int j = 0; j < bytes_per_channel; j += 2)
        {
            for (unsigned int i = 0; i < channels; i ++)
            {
                memcpy(&buf2[i*bytes_per_channel + j],
                       &buf[j*channels + i*2], 2);
            }
        }

        for (unsigned int i = 0; i < channels; i++)
        {
            // header (hist, step idx)
            size_t bytes_written = 
                fwrite(&buf2[i*bytes_per_channel + 0], 1, 4, infile);
            CHECK_FILE(bytes_written != 4, infile, "fwrite");
        }

        for (unsigned int j = 0; j < bytes_per_channel-4; j += 4)
        {
            for (unsigned int i = 0; i < channels; i++)
            {
                size_t bytes_written = 
                    fwrite(&buf2[i*bytes_per_channel + 4 + j], 1, 4, infile);
                CHECK_FILE(bytes_written != 4, infile, "fwrite");
            }
        }

        size -= block_size;
        offset += block_size;
    }

    free(buf);
    free(buf2);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr,"reinterleave RIFF 0x11 IMA ADPCM (2-byte interleave from FSB)\n");
        fprintf(stderr,"usage: fsb_ima_reinterleave file.WAV\n");
        exit(EXIT_FAILURE);
    }

    FILE *infile = fopen(argv[1], "r+b");
    CHECK_ERRNO(!infile, "fopen");

    unsigned char riff_head[12];

    get_bytes_seek(0, infile, riff_head, 12);
    CHECK_ERROR( memcmp(&riff_head[0],"RIFF",4) , "doesn't start with RIFF");

    const uint32_t riff_size = read_32_le(&riff_head[4]);

    CHECK_ERROR( memcmp(&riff_head[8],"WAVE",4) , "not WAVE type RIFF");

    const long wave_chunk_start = 12;
    const long wave_chunk_end = 8 + riff_size;
    long chunk_offset = wave_chunk_start;
    int channels = -1;
    int block_size = -1;
    while (chunk_offset < wave_chunk_end)
    {
        const uint32_t chunk_type = get_32_be_seek(chunk_offset,infile);
        const uint32_t chunk_size = get_32_le(infile);

        CHECK_ERROR((chunk_offset + 8 + chunk_size) < chunk_offset + 8 ||
                    (chunk_offset + 8 + chunk_size) > wave_chunk_end,
                    "chunk size out of range");

        switch (chunk_type)
        {
            case UINT32_C(0x666d7420):  // fmt
                CHECK_ERROR( (0xE > chunk_size), "fmt chunk too small" );
                CHECK_ERROR( (0x11 != get_16_le_seek(chunk_offset + 8 + 0, infile)), "not codec id 0x11" );
                channels = get_16_le_seek(chunk_offset + 8 + 2, infile);
                block_size = get_16_le_seek(chunk_offset + 8 + 0xc, infile);
                break;
            case UINT32_C(0x64617461):  // data
                CHECK_ERROR( -1 == channels || -1 == block_size, "data before fmt?" );
                reinterleave_ms_ima(infile, chunk_offset+8, chunk_size, channels, block_size);
                break;
            default:
                // ignore
                break;
        }

        chunk_offset += 8 + chunk_size;
    }

    CHECK_FILE( EOF == fclose(infile), infile, "fclose" );
}
