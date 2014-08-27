#include <stdio.h>

#include "cpk_uncompress.h"
#include "util.h"
#include "error_stuff.h"

const uint64_t CRILAYLA_sig = UINT64_C(0x4352494C41594C41);

// Decompress compressed segments in CRI CPK filesystems

long uncompress(reader_t *infile, long offset, long size, FILE *outfile);

#if 0
int main(int argc, char **argv)
{
    printf("cpk_uncompress 0.1\n\n");
    if (argc != 3)
        fprintf(stderr,"Incorrect program usage\n\nusage: %s input output\n",argv[0]);

    /* open input file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen input");

    /* open output file */
    FILE *outfile = fopen(argv[2], "w+b");
    CHECK_ERRNO(!outfile, "fopen output");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0, SEEK_END) != 0, "fseek");
    const long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    long uncompressed_size = 
        uncompress(infile, 0, file_length, outfile);

    CHECK_ERROR( uncompressed_size < 0,
        "uncompress failed");

    exit(EXIT_SUCCESS);
}
#endif

// only for up to 16 bits
static inline uint16_t get_next_bits(reader_t *infile, long * const offset_p, uint8_t * const bit_pool_p, int * const bits_left_p, const int bit_count)
{
    uint16_t out_bits = 0;
    int num_bits_produced = 0;
    while (num_bits_produced < bit_count)
    {
        if (0 == *bits_left_p)
        {
            *bit_pool_p = get_byte_seek(*offset_p, infile);
            *bits_left_p = 8;
            --*offset_p;
        }

        int bits_this_round;
        if (*bits_left_p > (bit_count - num_bits_produced))
            bits_this_round = bit_count - num_bits_produced;
        else
            bits_this_round = *bits_left_p;

        out_bits <<= bits_this_round;
        out_bits |=
            (*bit_pool_p >> (*bits_left_p - bits_this_round)) &
            ((1 << bits_this_round) - 1);

        *bits_left_p -= bits_this_round;
        num_bits_produced += bits_this_round;
    }

    return out_bits;
}

#define GET_NEXT_BITS(bit_count) get_next_bits(infile, &input_offset, &bit_pool, &bits_left, bit_count)

long uncompress(reader_t *infile, long offset, long input_size, FILE *outfile)
{
    unsigned char *output_buffer = NULL;
    CHECK_ERROR( !(
          (get_32_le_seek(offset+0x00, infile) == 0 &&
           get_32_le_seek(offset+0x04, infile) == 0) ||
          (get_64_be_seek(offset+0x00, infile) == CRILAYLA_sig)
        ), "didn't find 0 or CRILAYLA signature for compressed data");

    const long uncompressed_size = 
        get_32_le_seek(offset+0x08, infile);

    const long uncompressed_header_offset =
        offset + get_32_le_seek(offset+0x0C, infile)+0x10;

    CHECK_ERROR( uncompressed_header_offset + 0x100 != offset + input_size, "size mismatch");

    output_buffer = malloc(uncompressed_size + 0x100);
    CHECK_ERROR(!output_buffer, "malloc");

    get_bytes_seek(uncompressed_header_offset, infile, output_buffer, 0x100);

    const long input_end = offset + input_size - 0x100 - 1;
    long input_offset = input_end;
    const long output_end = 0x100 + uncompressed_size - 1;
    uint8_t bit_pool = 0;
    int bits_left = 0;
    long bytes_output = 0;

    while ( bytes_output < uncompressed_size )
    {
        if (GET_NEXT_BITS(1))
        {
            long backreference_offset =
                output_end-bytes_output+GET_NEXT_BITS(13)+3;
            long backreference_length = 3;

            // decode variable length coding for length
            enum { vle_levels = 4 };
            int vle_lens[vle_levels] = { 2, 3, 5, 8 };
            int vle_level;
            for (vle_level = 0; vle_level < vle_levels; vle_level++)
            {
                int this_level = GET_NEXT_BITS(vle_lens[vle_level]);
                backreference_length += this_level;
                if (this_level != ((1 << vle_lens[vle_level])-1)) break;
            }
            if (vle_level == vle_levels)
            {
                int this_level;
                do
                {
                    this_level = GET_NEXT_BITS(8);
                    backreference_length += this_level;
                } while (this_level == 255);
            }

            //printf("0x%08lx backreference to 0x%lx, length 0x%lx\n", output_end-bytes_output, backreference_offset, backreference_length);
            for (int i=0;i<backreference_length;i++)
            {
                output_buffer[output_end-bytes_output] = output_buffer[backreference_offset--];
                bytes_output++;
            }
        }
        else
        {
            // verbatim byte
            output_buffer[output_end-bytes_output] = GET_NEXT_BITS(8);
            //printf("0x%08lx verbatim byte\n", output_end-bytes_output);
            bytes_output++;
        }
    }

    put_bytes_seek(0, outfile, output_buffer, 0x100 + uncompressed_size);
    free(output_buffer);

    return 0x100 + bytes_output;
}
