#ifndef _UTIL_H_INCLUDED
#define _UTIL_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "error_stuff.h"

void dump(FILE *infile, FILE *outfile, long offset, size_t size);

uint32_t read_32_le(unsigned char bytes[4]);
uint16_t read_16_le(unsigned char bytes[2]);
uint64_t read_64_be(unsigned char bytes[8]);
uint32_t read_32_be(unsigned char bytes[4]);
uint16_t read_16_be(unsigned char bytes[2]);
void write_32_be(uint32_t value, unsigned char bytes[4]);
void write_32_le(uint32_t value, unsigned char bytes[4]);
void write_16_be(uint16_t value, unsigned char bytes[2]);

uint8_t get_byte(FILE *infile);
uint8_t get_byte_seek(long offset, FILE *infile);
uint16_t get_16_be(FILE *infile);
uint16_t get_16_be_seek(long offset, FILE *infile);
uint16_t get_16_le(FILE *infile);
uint16_t get_16_le_seek(long offset, FILE *infile);
uint32_t get_32_be(FILE *infile);
uint32_t get_32_be_seek(long offset, FILE *infile);
uint32_t get_32_le(FILE *infile);
uint32_t get_32_le_seek(long offset, FILE *infile);
uint64_t get_64_be(FILE *infile);
uint64_t get_64_be_seek(long offset, FILE *infile);
void get_bytes(FILE *infile, unsigned char *buf, size_t byte_count);
void get_bytes_seek(long offset, FILE *infile, unsigned char *buf, size_t byte_count);

void put_byte(uint8_t value, FILE *outfile);
void put_byte_seek(uint8_t value, long offset, FILE *outfile);
void put_16_be(uint16_t value, FILE *outfile);
void put_16_be_seek(uint16_t value, long offset, FILE *outfile);
void put_32_be(uint32_t value, FILE *outfile);
void put_32_be_seek(uint32_t value, long offset, FILE *outfile);
void put_32_le(uint32_t value, FILE *outfile);
void put_32_le_seek(uint32_t value, long offset, FILE *outfile);
void put_bytes(FILE *outfile, const unsigned char *buf, size_t byte_count);
void put_bytes_seek(long offset, FILE *outfile, const unsigned char *buf, size_t byte_count);

#define EXPECT_32_BE(offset, infile, value, name) do{\
    CHECK_ERROR(get_32_be_seek(offset, infile) != UINT32_C(value), "expected " name);\
}while(0)

#define INDENT_LEVEL 2
void fprintf_indent(FILE *outfile, int indent);

long read_long(char *text);

long pad(long current_offset, long pad_amount, FILE *outfile);

#endif /* _UTIL_H_INCLUDED */
