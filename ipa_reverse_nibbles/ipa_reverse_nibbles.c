#include <stdlib.h>
#include <stdio.h>
#include "error_stuff.h"
#include "util.h"

void reverse_nibbles(FILE *infile, long offset, size_t size)
{
    unsigned char buf[0x800];

    while (size > 0)
    {
        size_t bytes_to_fix = sizeof(buf);
        if (bytes_to_fix > size) bytes_to_fix = size;

        CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

        size_t bytes_read = fread(buf, 1, bytes_to_fix, infile);
        //fprintf(stderr,"offset=%lx bytes_to_fix=%lx bytes_read=%lx\n",offset,(long)bytes_to_fix,(long)bytes_read);
        CHECK_FILE(bytes_read != bytes_to_fix, infile, "fread");

        for (int i = 0; i < bytes_to_fix; i++)
        {
            buf[i] = ((buf[i] & 0xf0) >> 4) | ((buf[i] & 0x0f) << 4);
        }

        CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

        size_t bytes_written = fwrite(buf, 1, bytes_to_fix, infile);
        CHECK_FILE(bytes_written != bytes_to_fix, infile, "fwrite");

        size -= bytes_to_fix;
        offset += bytes_to_fix;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr,"usage: ipa_nibble_reverse file.WAV\n");
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
    while (chunk_offset < wave_chunk_end)
    {
        const uint32_t chunk_type = get_32_be_seek(chunk_offset,infile);
        const uint32_t chunk_size = get_32_le(infile);

        CHECK_ERROR((chunk_offset + 8 + chunk_size) < chunk_offset + 8 ||
                    (chunk_offset + 8 + chunk_size) > wave_chunk_end,
                    "chunk size out of range");

        switch (chunk_type)
        {
            case UINT32_C(0x64617461):  // data
                reverse_nibbles(infile, chunk_offset+8, chunk_size);
                break;
            default:
                // ignore
                break;
        }

        chunk_offset += 8 + chunk_size;
    }
}
