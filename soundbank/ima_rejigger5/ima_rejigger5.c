#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "error_stuff.h"
#include "util.h"

static int16_t const ima_steps[89] = { /* ~16-bit precision; 4 bit code */
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
  253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
  1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
  3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
  11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767};
static const int MAX_IMA_STATE = 88;

static int const state_adjust[16] = {
  -1,-1,-1,-1,2,4,6,8,
  -1,-1,-1,-1,2,4,6,8};


int samples_from_bytes(int block_size)
{
    // NOTE!!! This is one fewer sample than standard MS IMA would decode per frame.
    return (block_size - 4) * 2;
}

void decode_ms_ima(FILE *infile, bool big_endian, long offset, size_t size, unsigned int channels, unsigned int block_size, FILE *outfile)
{
    unsigned char buf[block_size];
    int_fast16_t hist[channels];
    unsigned char state[channels];
    const unsigned int bytes_per_channel = block_size / channels;
    CHECK_ERROR( (size % block_size), "block size doesn't go evenly into data size" );
    CHECK_ERROR( (block_size % channels), "block size doesn't divide evenly by channels" );
    CHECK_ERROR( (bytes_per_channel % 4), "expected multiple of 4 bytes per channel" );
    const unsigned int samples_per_block = samples_from_bytes(bytes_per_channel);
    unsigned char outbuf[samples_per_block*channels*2];

    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    while (size > 0)
    {
        size_t bytes_read = fread(buf, 1, block_size, infile);
        CHECK_FILE(bytes_read != block_size, infile, "fread");

        unsigned char * outbufp = outbuf;

        for (unsigned int c = 0; c < channels; c++)
        {
            if (big_endian)
            {
                hist[c] = (int16_t)read_16_be(buf+c*bytes_per_channel);
            }
            else
            {
                hist[c] = (int16_t)read_16_le(buf+c*bytes_per_channel);
            }

            write_16_le(hist[c], outbufp);
            outbufp += 2;

            state[c] = buf[c*bytes_per_channel + 2];
            CHECK_ERROR( (state[c] > MAX_IMA_STATE), "bad step idx in header" );

            CHECK_ERROR( (buf[c*bytes_per_channel + 4 + (samples_per_block-1)/2] >> 4) != 0 , "nonzero padding nibble found");
        }

        for (unsigned int i = 1; i < samples_per_block; i++)
        {
            for (unsigned int c = 0; c < channels; c++)
            {
                int_fast32_t sample = hist[c];

                unsigned char s = buf[c*bytes_per_channel + 4 + (i-1)/2];
                if (i & 1)
                {
                    s &= 15;
                }
                else
                {
                    s >>= 4;
                }

                uint_fast32_t scale = ima_steps[state[c]];
                uint_fast32_t delta = scale >> 3;

                if (s & 4)
                {
                  delta += scale;
                }
                if (s & 2)
                {
                  delta += scale >> 1;
                }
                if (s & 1)
                {
                  delta += scale >> 2;
                }

                if (s & 8)
                {
                  sample -= delta;
                  if (sample < -0x8000)
                  {
                    sample = -0x8000;
                  }
                }
                else
                {
                  sample += delta;
                  if (sample > 0x7fff)
                  {
                    sample = 0x7fff;
                  }
                }

                hist[c] = sample;
                write_16_le(hist[c], outbufp);
                outbufp += 2;

                int next_state = state[c];
                next_state += state_adjust[s];
                if (next_state < 0)
                {
                    next_state = 0;
                }
                else if (next_state > MAX_IMA_STATE) 
                {
                    next_state = MAX_IMA_STATE;
                }
                state[c] = next_state;
            } // end sample per channel loop

        } // end sample loop

        CHECK_ERROR(outbufp - outbuf != samples_per_block*2*channels, "sample count miswrite");
        size_t bytes_written = 
            fwrite(outbuf, 1, outbufp-outbuf, outfile);
        CHECK_FILE(bytes_written != outbufp-outbuf, outfile, "fwrite");

        size -= block_size;
    } // end block loop
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr,"decode Wwise RIFF/RIFX 0x2 uninterleaved MS IMA ADPCM\n");
        fprintf(stderr,"usage: ima_rejigger5 file.wem file.wav\n");
        exit(EXIT_FAILURE);
    }

    FILE *infile = fopen(argv[1], "rb");
    FILE *outfile = fopen(argv[2], "wb");
    CHECK_ERRNO(!infile, "fopen");

    unsigned char riff_head[12];

    get_bytes_seek(0, infile, riff_head, 12);

    bool big_endian = false;
    if (!memcmp(&riff_head[0],"RIFX",4))
    {
      big_endian = true;
    }
    else if (!memcmp(&riff_head[0],"RIFF",4))
    {
      // nothing
    }
    else
    {
      CHECK_ERROR( 1 , "doesn't start with RIFF or RIFX");
    }

    const uint32_t riff_size = big_endian ? read_32_be(&riff_head[4]) :
                                            read_32_le(&riff_head[4]);

    CHECK_ERROR( memcmp(&riff_head[8],"WAVE",4) , "not WAVE type RIFF");

    const long wave_chunk_start = 12;
    const long wave_chunk_end = 8 + riff_size;
    long chunk_offset = wave_chunk_start;
    int channels = -1;
    int block_size = -1;
    uint32_t sample_rate = 0;
    while (chunk_offset < wave_chunk_end)
    {
        const uint32_t chunk_type = get_32_be_seek(chunk_offset,infile);
        const uint32_t chunk_size = big_endian ? get_32_be(infile) :
                                                 get_32_le(infile);

        CHECK_ERROR((chunk_offset + 8 + chunk_size) < chunk_offset + 8 ||
                    (chunk_offset + 8 + chunk_size) > wave_chunk_end,
                    "chunk size out of range");

        switch (chunk_type)
        {
            case UINT32_C(0x666d7420):  // fmt
                CHECK_ERROR( (0x18 != chunk_size), "fmt chunk wrong size" );
                CHECK_ERROR( (0x2 != (big_endian ? get_16_be_seek(chunk_offset + 8 + 0, infile) :
                                                   get_16_le_seek(chunk_offset + 8 + 0, infile)) ), "not codec id 0x2" );
                sample_rate = big_endian ? get_32_be_seek(chunk_offset + 8 + 4, infile) :
                                           get_32_le_seek(chunk_offset + 8 + 4, infile);
                channels = big_endian ? get_16_be_seek(chunk_offset + 8 + 2, infile) :
                                        get_16_le_seek(chunk_offset + 8 + 2, infile);
                block_size = big_endian ? get_16_be_seek(chunk_offset + 8 + 0xc, infile) :
                                          get_16_le_seek(chunk_offset + 8 + 0xc, infile);

                break;
            case UINT32_C(0x64617461):  // data
                CHECK_ERROR( -1 == channels || -1 == block_size, "data before fmt?" );

                // write header
                {
                  const uint32_t data_size = chunk_size/block_size * samples_from_bytes(block_size / channels) * channels * 2;
                  const uint32_t riff_size = 4 + (8 + 16) + (8 + data_size);
                  put_bytes(outfile, "RIFF", 4);
                  put_32_le(riff_size, outfile);
                  put_bytes(outfile, "WAVE", 4);

                  put_bytes(outfile, "fmt ", 4);
                  put_32_le(16, outfile);
                  put_16_le(1, outfile);    // PCM
                  put_16_le(channels, outfile);
                  put_32_le(sample_rate, outfile);
                  put_32_le(sample_rate*channels*2, outfile);   // 2 bytes per sample
                  put_16_le(channels*2, outfile);               // 2 bytes per sample
                  put_16_le(16, outfile);                       // 16 bits per samples

                  put_bytes(outfile, "data", 4);
                  put_32_le(data_size, outfile);
                }

                // write data
                decode_ms_ima(infile, big_endian, chunk_offset+8, chunk_size, channels, block_size, outfile);
                break;
            default:
                // ignore
                break;
        }

        chunk_offset += 8 + chunk_size;
    }

    CHECK_FILE( EOF == fclose(outfile), outfile, "fclose" );
    CHECK_FILE( EOF == fclose(infile), infile, "fclose" );
}
