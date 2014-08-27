#ifndef _BITSTREAM_H_INCLUDED
#define _BITSTREAM_H_INCLUDED

#include <stdint.h>

// MSB first

// bitstream reading
struct bitstream_reader;

struct bitstream_reader *init_bitstream_reader(const uint8_t *pool, size_t pool_size, size_t consecutive_bits, size_t skip_bits);
unsigned int get_bit(struct bitstream_reader *bs);
uint32_t get_bits(struct bitstream_reader *bs, unsigned int bits);
void free_bitstream_reader(struct bitstream_reader *bs);

// bitstream writing
struct bitstream_writer;

struct bitstream_writer *init_bitstream_writer(FILE *outfile);
void put_bit(struct bitstream_writer *bs, unsigned int val);
void put_bits(struct bitstream_writer *bs, uint32_t val, unsigned int bits);
void flush_bitstream_writer(struct bitstream_writer *bs);
void free_bitstream_writer(struct bitstream_writer *bs);

#endif /* _BISTREAM_H_INCLUDED */
