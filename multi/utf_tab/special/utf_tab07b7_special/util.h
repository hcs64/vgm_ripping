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

struct reader_s;
typedef struct reader_s reader_t;

void make_directory(const char *name);

FILE * open_file_in_directory(const char *base_name, const char *dir_name, const char orig_sep, const char *file_name, const char *perms);

char * number_name(const char * name_head, const char * name_tail, unsigned int id, unsigned int max_id);

const char * strip_path(const char * path);

reader_t * open_reader_file(const char *file_name);
reader_t * open_reader_crypt(reader_t *infile, long base_offset, uint8_t start, uint8_t mult);
int fseek_reader(reader_t *r, long offset);
long reader_length(reader_t *r);
long reader_tell(reader_t *r);

void close_reader(reader_t *r);

void dump(reader_t *infile, FILE *outfile, long offset, size_t size);

void dump_from_here(reader_t *infile, FILE *outfile, size_t size);

uint32_t read_32_le(unsigned char bytes[4]);
uint16_t read_16_le(unsigned char bytes[2]);
uint64_t read_64_be(unsigned char bytes[8]);
uint32_t read_32_be(unsigned char bytes[4]);
uint16_t read_16_be(unsigned char bytes[2]);
void write_32_be(uint32_t value, unsigned char bytes[4]);
void write_32_le(uint32_t value, unsigned char bytes[4]);
void write_16_be(uint16_t value, unsigned char bytes[2]);
void write_16_le(uint16_t value, unsigned char bytes[2]);

uint8_t get_byte(reader_t *infile);
uint8_t get_byte_seek(long offset, reader_t *infile);
uint16_t get_16_be(reader_t *infile);
uint16_t get_16_be_seek(long offset, reader_t *infile);
uint16_t get_16_le(reader_t *infile);
uint16_t get_16_le_seek(long offset, reader_t *infile);
uint32_t get_32_be(reader_t *infile);
uint32_t get_32_be_seek(long offset, reader_t *infile);
uint32_t get_32_le(reader_t *infile);
uint32_t get_32_le_seek(long offset, reader_t *infile);
uint64_t get_64_be(reader_t *infile);
uint64_t get_64_be_seek(long offset, reader_t *infile);
void get_bytes(reader_t *infile, unsigned char *buf, size_t byte_count);
void get_bytes_seek(long offset, reader_t *infile, unsigned char *buf, size_t byte_count);

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

#define INDENT_LEVEL 2
void fprintf_indent(FILE *outfile, int indent);

long read_long(char *text);

long pad(long current_offset, long pad_amount, FILE *outfile);

#endif /* _UTIL_H_INCLUDED */
