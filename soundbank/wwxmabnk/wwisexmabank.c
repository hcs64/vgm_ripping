#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "util.h"
#include "error_stuff.h"

// WWise funky RIFX

void handle_RIFX(uint32_t RIFX_size, FILE * infile, const char * outfile_prefix, int file_idx);
uint8_t *make_xma_header(uint32_t srate, uint32_t size, int channels);

const int xma_header_size = 0x3c;

int main(int argc, char ** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s infile.WwiseBank outfile_prefix\n", argv[0]);
        return 1;
    }

    FILE *infile = fopen(argv[1], "rb");

    CHECK_ERRNO(!infile, "failed opening input");

    const char * outfile_prefix = argv[2];

    CHECK_ERROR(UINT32_C(-1) != get_32_be(infile), "missing magic -1");


    long DATA_offset = -1;
    uint32_t DATA_size = 0;

    {
        int done = 0;
        long chunk_start = 0x80;

        // top level wwise chunks loop
        while (!done)
        {
            CHECK_ERRNO(-1 == fseek(infile, chunk_start, SEEK_SET), "fseek to BKHD chunk failed");

            const uint32_t chunk_id = get_32_be(infile);
            const uint32_t chunk_size = get_32_be(infile);

            switch (chunk_id)
            {
                case UINT32_C(0x424B4844):  // BKHD: Bank Header?
                case UINT32_C(0x44494458):  // DIDX: Data Index?
                case UINT32_C(0x48495243):  // HIRC
                case UINT32_C(0x53544944):  // STID: Stream Identifier?
                    break;
                case UINT32_C(0):
                    // 16 bytes padding at end, starting with 0s
                    done = 1;
                    break;
                case UINT32_C(0x44415441):  // DATA: body
                    DATA_offset = chunk_start + 8;
                    DATA_size = chunk_size;
                    break;
                default:
                    CHECK_ERROR(1, "unknown chunk id");
                    break;
            }

            chunk_start += 8 + chunk_size;
        }

        CHECK_ERROR(DATA_offset == -1, "no DATA chunk found in BKHD");
    }

    {
        int file_idx = 0;

        long subfile_offset = DATA_offset;

        // subfile loop
        while (subfile_offset < DATA_offset + DATA_size)
        {
            CHECK_ERRNO(-1 == fseek(infile, subfile_offset, SEEK_SET), "fseek to subfile failed");

            const uint32_t subfile_id = get_32_be(infile);
            const uint32_t subfile_size = get_32_le(infile);    // SAY WHAT?!?
            switch (subfile_id)
            {
                case UINT32_C(0x52494658):  // RIFX
                    break;
                default:
                    CHECK_ERROR(1, "unexpected subfile type");
                    break;
            }

            handle_RIFX(subfile_size, infile, outfile_prefix, file_idx);
            subfile_offset += subfile_size;
            file_idx ++;
        }
    }

    return 0;
}

void handle_RIFX(uint32_t RIFX_size, FILE * infile, const char * outfile_prefix, int file_idx)
{
    CHECK_ERROR(UINT32_C(0x57415645) != get_32_be(infile), "missing WAVE on RIFX");
    RIFX_size -= 12;

    uint16_t codec_id = 0;
    uint16_t channels = 0;
    uint32_t srate = 0;

    long data_offset = -1;
    uint32_t data_size = 0;

    while (RIFX_size > 0)
    {
        const uint32_t chunk_id = get_32_be(infile);
        const uint32_t chunk_size = get_32_be(infile);

        switch (chunk_id)
        {
            case UINT32_C(0x666d7420):  // fmt
            {
                CHECK_ERROR(chunk_size != 0x34, "fmt chunk size not 0x34");

                codec_id = get_16_be(infile);
                channels = get_16_be(infile);
                srate = get_32_be(infile);

                if (codec_id != 0x166)
                {
                    printf("File %5d: codec id 0x%"PRIx16", skipping\n", file_idx, codec_id);
                    return;
                }

                CHECK_ERRNO(-1 == fseek(infile, chunk_size-8, SEEK_CUR), "fseek past RIFX chunk failed");
                break;
            }
            case UINT32_C(0x7365656b):  // seek
            case UINT32_C(0x584d4163):  // XMAc
            case UINT32_C(0x4a554e4b):  // JUNK
                CHECK_ERRNO(-1 == fseek(infile, chunk_size, SEEK_CUR), "fseek past RIFX chunk failed");
                break;
            case UINT32_C(0x64617461):  // data
                data_offset = ftell(infile);
                data_size = chunk_size;
                break;
            default:
                CHECK_ERROR(1, "unexpected RIFX chunk type");
                break;
        }

        RIFX_size -= 8 + chunk_size;
    }

    CHECK_ERROR(data_size == -1, "no RIFX data chunk");
    CHECK_ERROR(codec_id != 0x166, "no RIFX codec chunk");

    FILE * outfile = NULL;
    {
        char * outfile_name = number_name(outfile_prefix, ".xma", file_idx, 10000);
        outfile  = fopen(outfile_name, "wb");
        CHECK_ERRNO(!outfile, "output open failed");

        printf("%s: %"PRIu32" bytes, %"PRIu16" channels, %"PRIu32" Hz\n", outfile_name, data_size, channels, srate);

        free(outfile_name); outfile_name = NULL;
    }

    {
        uint8_t * header = make_xma_header(srate, data_size, channels);

        put_bytes(outfile, header, xma_header_size);

        free(header); header = NULL;
    }

    dump(infile, outfile, data_offset, data_size);

    CHECK_ERRNO(EOF == fclose(outfile), "fclose outfile");

    outfile = NULL;
}

uint8_t *make_xma_header(uint32_t srate, uint32_t size, int channels)
{
    uint8_t *h = malloc(xma_header_size);
    CHECK_ERRNO(!h, "malloc");

    // RIFF header
    memcpy(&h[0x00], "RIFF", 4);
    write_32_le(size+0x3c-8, &h[0x04]); // RIFF size
    memcpy(&h[0x08], "WAVE", 4);

    // fmt chunk
    memcpy(&h[0x0C], "fmt ", 4);
    write_32_le(0x20, &h[0x10]);    // fmt chunk size

    write_16_le(0x165, &h[0x14]);   // WAVE_FORMAT_XMA
    write_16_le(16, &h[0x16]);      // 16 bits per sample
    write_16_le(0, &h[0x18]);       // encode options **
    write_16_le(0, &h[0x1a]);       // largest skip
    write_16_le(1, &h[0x1c]);       // # streams
    h[0x1e] = 0;    // loops
    h[0x1f] = 3;    // encoder version

    // lone stream info
    write_32_le(0, &h[0x20]);       // bytes per second **
    write_32_le(srate, &h[0x24]);   // sample rate

    write_32_le(0, &h[0x28]);       // loop start
    write_32_le(0, &h[0x2c]);       // loop end
    h[0x30] = 0;    // subframe loop data

    h[0x31] = channels;             // channels
    write_16_le(0x0002, &h[0x32]);  // channel mask

    // data chunk
    memcpy(&h[0x34], "data", 4);
    write_32_le(size, &h[0x38]);    // data chunk size

    return h;
}

