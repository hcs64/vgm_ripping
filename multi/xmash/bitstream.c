#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include "error_stuff.h"
#include "util.h"
#include "bitstream.h"

struct bitstream_reader
{
    const uint8_t *pool;
    size_t pool_size;

    // parameters of the layout
    size_t consecutive_bits;
    size_t skip_bits;

    // current state
    size_t consecutive_bits_left;
    uint8_t bit_buffer;
    unsigned int bits_left; // in buffer
};

struct bitstream_reader *init_bitstream_reader(const uint8_t *pool, size_t pool_size, size_t consecutive_bits, size_t skip_bits)
{
    struct bitstream_reader *bs = malloc(sizeof(struct bitstream_reader));
    CHECK_ERRNO(!bs, "malloc");

    bs->pool = pool;
    bs->pool_size = pool_size;
    bs->consecutive_bits = bs->consecutive_bits_left = consecutive_bits;
    bs->skip_bits = skip_bits;

    bs->bits_left = 0;

    return bs;
}

unsigned int get_bit(struct bitstream_reader *bs)
{
    if (bs->consecutive_bits && bs->consecutive_bits_left == 0)
    {
        // hit the end of a packet, skip over the skip bits
        bs->consecutive_bits_left = bs->skip_bits;
        // recursive call to throw away bits
        while (bs->consecutive_bits_left) get_bit(bs);

        bs->consecutive_bits_left = bs->consecutive_bits;
    }

    if (0 == bs->bits_left)
    {
        CHECK_ERROR(0 == bs->pool_size, "bitstream underflow");
        bs->bit_buffer = *(bs->pool++);
        bs->bits_left = 8;

        bs->pool_size --;
    }
    bs->bits_left --;
    if (bs->consecutive_bits) bs->consecutive_bits_left --;
    return ( ( bs->bit_buffer & ( 1 << bs->bits_left ) ) != 0);
}

uint32_t get_bits(struct bitstream_reader *bs, unsigned int bits)
{
    CHECK_ERROR( bits > 32, "max 32 bits" );

    uint32_t total = 0;

    for (unsigned int i = 0; i < bits; i++)
    {
        total <<= 1;

        if (get_bit(bs))
        {
            total |= 1;
        }
    }

    return total;
}

void free_bitstream_reader(struct bitstream_reader *bs)
{
    free(bs);
}

struct bitstream_writer
{
    FILE *outfile;

    uint8_t bit_buffer;
    unsigned int bits_used;
};

////////////////

struct bitstream_writer *init_bitstream_writer(FILE *outfile)
{
    struct bitstream_writer *bs = malloc(sizeof(struct bitstream_writer));

    CHECK_ERRNO(!bs, "malloc");

    bs->outfile = outfile;
    
    bs->bit_buffer = 0;
    bs->bits_used = 0;

    return bs;
}

void put_bit(struct bitstream_writer *bs, unsigned int val)
{
    if (8 == bs->bits_used)
    {
        flush_bitstream_writer(bs);
    }

    if (val)
    {
        bs->bit_buffer |= (0x80 >> bs->bits_used);
    }

    bs->bits_used ++;
}

void put_bits(struct bitstream_writer *bs, uint32_t val, unsigned int bits)
{
    CHECK_ERROR( bits > 32, "max 32 bits" );

    for (unsigned int i = 0; i < bits; i++)
    {
        put_bit(bs, val & (1 << (bits-1-i)));
    }
}

void flush_bitstream_writer(struct bitstream_writer *bs)
{
    if (0 != bs->bits_used)
    {
        int rc = fputc(bs->bit_buffer, bs->outfile);
        CHECK_FILE(EOF == rc, bs->outfile, "fputc");

        bs->bit_buffer = 0;
        bs->bits_used = 0;
    }
}

void free_bitstream_writer(struct bitstream_writer *bs)
{
    free(bs);
}
