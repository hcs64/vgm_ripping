#include <stdio.h>

#include "util.h"
#include "error_stuff.h"

void analyze_Huf8(FILE *infile, FILE *outfile, long file_length);

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("puff8 0.0 - Huf8 decoder\n\n");
        printf("Usage: %s infile outfile\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen");

    FILE *outfile = fopen(argv[2], "wb");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    analyze_Huf8(infile, outfile, file_length);

    CHECK_ERRNO(fclose(outfile) == EOF, "fclose");

    exit(EXIT_SUCCESS);
}

void analyze_Huf8(FILE *infile, FILE *outfile, long file_length)
{
    unsigned char *decode_table = NULL;
    int decode_table_size;
    long decoded_length;
    int symbol_count;

    /* read header */
    {
        unsigned char buf[5];
        get_bytes_seek(0, infile, buf, 5);
        CHECK_ERROR (buf[0] != 0x28, "not 8-bit Huffman");
        decoded_length = read_24_le(&buf[1]);
        symbol_count = buf[4] + 1;
    }

    /* allocate decode table */
    decode_table_size = symbol_count * 2 - 1;
    decode_table = malloc(decode_table_size);
    CHECK_ERRNO(decode_table == NULL, "malloc");

    /* read decode table */
    get_bytes(infile, decode_table, decode_table_size);

#if 0
    printf("encoded size = %ld bytes (%d header + %ld body)\n",
            file_length, 5 + decode_table_size,
            file_length - (5 + decode_table_size));
    printf("decoded size = %ld bytes\n", decoded_length);
#endif

    /* decode */
    {
        uint32_t bits;
        int bits_left = 0;
        int table_offset = 0;
        long bytes_decoded = 0;

        while ( bytes_decoded < decoded_length )
        {
            if (bits_left == 0)
            {
                bits = get_32_le(infile);
                bits_left = 32;
            }

            int current_bit = ((bits & 0x80000000) != 0);
            int next_offset = ((table_offset + 1) / 2 * 2) + 1 +
                (decode_table[table_offset] & 0x3f) * 2 +
                (current_bit ? 1 : 0);

#if 0
            printf("%d %02x %lx => %lx\n", current_bit,
                    decode_table[table_offset],
                    (unsigned long)table_offset,
                    (unsigned long)next_offset);
#endif

            CHECK_ERROR (next_offset >= decode_table_size,
                    "reading past end of decode table");

            if ((!current_bit && (decode_table[table_offset] & 0x80)) ||
                ( current_bit && (decode_table[table_offset] & 0x40)))
            {
                CHECK_FILE(
                    fwrite(&decode_table[next_offset], 1, 1, outfile) != 1,
                    outfile, "fwrite");
                bytes_decoded++;
#if 0
                printf("%02x\n", decode_table[next_offset]);
                return;
#endif
                next_offset = 0;
            }

            CHECK_ERROR (next_offset == table_offset,
                    "stuck in a loop somehow");
            table_offset = next_offset;
            bits_left--;
            bits <<= 1;
        }
    }

#if 0
    printf("done\n");
#endif
}
