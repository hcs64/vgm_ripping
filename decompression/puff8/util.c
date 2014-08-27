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
    for (int i=4-1; i>=0; i--) result = (result << 8) | bytes[i];
    return result;
}
uint32_t read_24_le(unsigned char bytes[3])
{
    uint32_t result = 0;
    for (int i=3-1; i>=0; i--) result = (result << 8) | bytes[i];
    return result;
}
uint16_t read_16_le(unsigned char bytes[2])
{
    uint32_t result = 0;
    for (int i=2-1; i>=0; i--) result = (result << 8) | bytes[i];
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
uint32_t get_32_le(FILE *infile)
{
    unsigned char buf[4];
    size_t bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");

    return read_32_le(buf);
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
