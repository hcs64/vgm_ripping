#ifndef _UTIL_H_INCLUDED
#define _UTIL_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "error_stuff.h"

#ifdef __MINGW32__
#define DIRSEP '\\'
#else
#define DIRSEP '/'
#endif

// endian-neutral integer reads
uint32_t read_32_le(const unsigned char bytes[4]);
uint16_t read_16_le(const unsigned char bytes[2]);
uint64_t read_64_be(const unsigned char bytes[8]);
uint32_t read_32_be(const unsigned char bytes[4]);
uint16_t read_16_be(const unsigned char bytes[2]);
// endian-neutral integer writes
void write_32_be(uint32_t value, unsigned char bytes[4]);
void write_32_le(uint32_t value, unsigned char bytes[4]);
void write_16_be(uint16_t value, unsigned char bytes[2]);
void write_16_le(uint16_t value, unsigned char bytes[2]);

// self-checking file reads
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

uint8_t *get_whole_file(FILE *infile, long *file_size_p);

#define EXPECT_32_BE(data, value, name) do{\
    CHECK_ERROR(read_32_be(data) != UINT32_C(value), "expected " name);\
}while(0)

#define EXPECT_32_LE(data, value, name) do{\
    CHECK_ERROR(read_32_le(data) != UINT32_C(value), "expected " name);\
}while(0)

#define EXPECT_16_LE(data, value, name) do{\
    CHECK_ERROR(read_16_le(data) != UINT16_C(value), "expected " name);\
}while(0)

// self-checking file writes 
void put_byte(uint8_t value, FILE *outfile);
void put_byte_seek(uint8_t value, long offset, FILE *outfile);
void put_16_be(uint16_t value, FILE *outfile);
void put_16_be_seek(uint16_t value, long offset, FILE *outfile);
void put_16_le(uint16_t value, FILE *outfile);
void put_16_le_seek(uint16_t value, long offset, FILE *outfile);
void put_32_be(uint32_t value, FILE *outfile);
void put_32_be_seek(uint32_t value, long offset, FILE *outfile);
void put_32_le(uint32_t value, FILE *outfile);
void put_32_le_seek(uint32_t value, long offset, FILE *outfile);
void put_bytes(FILE *outfile, const unsigned char *buf, size_t byte_count);
void put_bytes_seek(long offset, FILE *outfile, const unsigned char *buf, size_t byte_count);

// self-checking wrapper for strtol
// not const due to strtol's 2nd arg
long read_long(char *text);

// dump a section of file
void dump(FILE *infile, FILE *outfile, long offset, size_t size);

// pad a file out to some multiple, conservatively
long pad(long current_offset, long pad_amount, FILE *outfile);

// create a directory
void make_directory(const char *name);

// open a binary file for writing in a directory, creating directories as needed
// original path may contain directories, orig_sep is the separator used (this is
// converted to DIRSEP)
FILE * open_file_in_directory(const char *base_name, const char *dir_name, const char orig_sep, const char *file_name, const char *perms);

// create an allocate a name built from numbers
char * number_name(const char * name_head, const char * name_tail, unsigned int id, unsigned int max_id);

// given a path to a file, return the filename part of the path
const char * strip_path(const char * path);

// 
char * copy_string(const char * s);

#endif /* _UTIL_H_INCLUDED */
