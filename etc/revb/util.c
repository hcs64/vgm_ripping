#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "error_stuff.h"
#include "util.h"

void dump(FILE *infile, FILE *outfile, long offset, size_t size)
{
    unsigned char buf[0x800];

    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    while (size > 0)
    {
        size_t bytes_to_copy = sizeof(buf);
        if (bytes_to_copy > size) bytes_to_copy = size;

        size_t bytes_read = fread(buf, 1, bytes_to_copy, infile);
        CHECK_FILE(bytes_read != bytes_to_copy, infile, "fread");

        size_t bytes_written = fwrite(buf, 1, bytes_to_copy, outfile);
        CHECK_FILE(bytes_written != bytes_to_copy, outfile, "fwrite");

        size -= bytes_to_copy;
    }
}

uint32_t read_32_le(unsigned char bytes[4])
{
    uint32_t result = 0;
    for (int i=3; i>=0; i--) result = (result << 8) | bytes[i];
    return result;
}
uint16_t read_16_le(unsigned char bytes[2])
{
    uint32_t result = 0;
    for (int i=1; i>=0; i--) result = (result << 8) | bytes[i];
    return result;
}
uint64_t read_64_be(unsigned char bytes[8])
{
    uint64_t result = 0;
    for (int i=0; i<8; i++) result = (result << 8) | bytes[i];
    return result;
}
uint32_t read_32_be(unsigned char bytes[4])
{
    uint32_t result = 0;
    for (int i=0; i<4; i++) result = (result << 8) | bytes[i];
    return result;
}
uint16_t read_16_be(unsigned char bytes[2])
{
    uint32_t result = 0;
    for (int i=0; i<2; i++) result = (result << 8) | bytes[i];
    return result;
}

void write_32_be(uint32_t value, unsigned char bytes[4])
{
    for (int i=3; i>=0; i--, value >>= 8) bytes[i] = value & 0xff;
}

void write_16_be(uint16_t value, unsigned char bytes[2])
{
    for (int i=1; i>=0; i--, value >>= 8) bytes[i] = value & 0xff;
}

uint8_t get_byte(FILE *infile)
{
    unsigned char buf[1];

    size_t bytes_read = fread(buf, 1, 1, infile);
    CHECK_FILE(bytes_read != 1, infile, "fread");

    return buf[0];
}
uint8_t get_byte_seek(long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    return get_byte(infile);
}

uint16_t get_16_be(FILE *infile)
{
    unsigned char buf[2];
    size_t bytes_read = fread(buf, 1, 2, infile);
    CHECK_FILE(bytes_read != 2, infile, "fread");

    return read_16_be(buf);
}
uint16_t get_16_be_seek(long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    return get_16_be(infile);
}
uint32_t get_32_be(FILE *infile)
{
    unsigned char buf[4];
    size_t bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");

    return read_32_be(buf);
}
uint32_t get_32_be_seek(long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    return get_32_be(infile);
}
uint64_t get_64_be(FILE *infile)
{
    unsigned char buf[8];
    size_t bytes_read = fread(buf, 1, 8, infile);
    CHECK_FILE(bytes_read != 8, infile, "fread");

    return read_64_be(buf);
}
uint64_t get_64_be_seek(long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    return get_64_be(infile);
}

void get_bytes(FILE *infile, unsigned char *buf, size_t byte_count)
{
    size_t bytes_read = fread(buf, 1, byte_count, infile);
    CHECK_FILE(bytes_read != byte_count, infile, "fread");
}

void get_bytes_seek(long offset, FILE *infile, unsigned char *buf, size_t byte_count)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");
    get_bytes(infile, buf, byte_count);
}

void put_byte(uint8_t value, FILE *infile)
{
    unsigned char buf[1];

    buf[0] = value;
    size_t bytes_written = fwrite(buf, 1, 1, infile);
    CHECK_FILE(bytes_written != 1, infile, "fwrite");
}
void put_byte_seek(uint8_t value, long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    put_byte(value, infile);
}

void put_16_be(uint16_t value, FILE *infile)
{
    unsigned char buf[2];
    write_16_be(value, buf);
    size_t bytes_written = fwrite(buf, 1, 2, infile);
    CHECK_FILE(bytes_written != 2, infile, "fwrite");
}
void put_16_be_seek(uint16_t value, long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    put_16_be(value, infile);
}
void put_32_be(uint32_t value, FILE *infile)
{
    unsigned char buf[4];
    write_32_be(value, buf);
    size_t bytes_written = fwrite(buf, 1, 4, infile);
    CHECK_FILE(bytes_written != 4, infile, "fwrite");
}
void put_32_be_seek(uint32_t value, long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    put_32_be(value, infile);
}
void put_bytes(FILE *infile, const unsigned char *buf, size_t byte_count)
{
    size_t bytes_written = fwrite(buf, 1, byte_count, infile);
    CHECK_FILE(bytes_written != byte_count, infile, "fwrite");
}

void put_bytes_seek(long offset, FILE *infile, const unsigned char *buf, size_t byte_count)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");
    put_bytes(infile, buf, byte_count);
}

void fprintf_indent(FILE *outfile, int indent)
{
        fprintf(outfile, "%*s",indent,"");
}

long read_long(char *text)
{
    char *endptr;

    errno = 0;
    long result = strtol(text, &endptr, 0);

    CHECK_ERRNO( errno != 0, "strtol" );
    CHECK_ERROR(*endptr != '\0', "bad number format");

    return result;
}

long pad(long current_offset, long pad_amount, FILE *outfile)
{
    long new_offset = (current_offset + pad_amount-1) / pad_amount * pad_amount;

    for (; current_offset < new_offset; current_offset++)
    {
        put_byte_seek(0, current_offset, outfile);
    }

    return new_offset;
}
