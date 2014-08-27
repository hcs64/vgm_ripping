#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#ifdef __MINGW32__
#include <io.h>
#endif
#include <sys/stat.h>

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

uint32_t read_32_le(const unsigned char bytes[4])
{
    uint32_t result = 0;
    for (int i=3; i>=0; i--) result = (result << 8) | bytes[i];
    return result;
}
uint16_t read_16_le(const unsigned char bytes[2])
{
    uint32_t result = 0;
    for (int i=1; i>=0; i--) result = (result << 8) | bytes[i];
    return result;
}
uint64_t read_64_be(const unsigned char bytes[8])
{
    uint64_t result = 0;
    for (int i=0; i<8; i++) result = (result << 8) | bytes[i];
    return result;
}
uint32_t read_32_be(const unsigned char bytes[4])
{
    uint32_t result = 0;
    for (int i=0; i<4; i++) result = (result << 8) | bytes[i];
    return result;
}
uint16_t read_16_be(const unsigned char bytes[2])
{
    uint32_t result = 0;
    for (int i=0; i<2; i++) result = (result << 8) | bytes[i];
    return result;
}

void write_32_be(uint32_t value, unsigned char bytes[4])
{
    for (int i=3; i>=0; i--, value >>= 8) bytes[i] = value & 0xff;
}

void write_32_le(uint32_t value, unsigned char bytes[4])
{
    for (int i=0; i<4; i++, value >>= 8) bytes[i] = value & 0xff;
}

void write_16_be(uint16_t value, unsigned char bytes[2])
{
    for (int i=1; i>=0; i--, value >>= 8) bytes[i] = value & 0xff;
}

void write_16_le(uint16_t value, unsigned char bytes[2])
{
    for (int i=0; i<2; i++, value >>= 8) bytes[i] = value & 0xff;
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
uint16_t get_16_le(FILE *infile)
{
    unsigned char buf[2];
    size_t bytes_read = fread(buf, 1, 2, infile);
    CHECK_FILE(bytes_read != 2, infile, "fread");

    return read_16_le(buf);
}
uint16_t get_16_le_seek(long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    return get_16_le(infile);
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
uint32_t get_32_le(FILE *infile)
{
    unsigned char buf[4];
    size_t bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");

    return read_32_le(buf);
}
uint32_t get_32_le_seek(long offset, FILE *infile)
{
    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    return get_32_le(infile);
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

void put_byte(uint8_t value, FILE *outfile)
{
    unsigned char buf[1];

    buf[0] = value;
    size_t bytes_written = fwrite(buf, 1, 1, outfile);
    CHECK_FILE(bytes_written != 1, outfile, "fwrite");
}
void put_byte_seek(uint8_t value, long offset, FILE *outfile)
{
    CHECK_ERRNO(fseek(outfile, offset, SEEK_SET) != 0, "fseek");

    put_byte(value, outfile);
}

void put_16_be(uint16_t value, FILE *outfile)
{
    unsigned char buf[2];
    write_16_be(value, buf);
    size_t bytes_written = fwrite(buf, 1, 2, outfile);
    CHECK_FILE(bytes_written != 2, outfile, "fwrite");
}
void put_16_be_seek(uint16_t value, long offset, FILE *outfile)
{
    CHECK_ERRNO(fseek(outfile, offset, SEEK_SET) != 0, "fseek");

    put_16_be(value, outfile);
}
void put_16_le(uint16_t value, FILE *outfile)
{
    unsigned char buf[2];
    write_16_le(value, buf);
    size_t bytes_written = fwrite(buf, 1, 2, outfile);
    CHECK_FILE(bytes_written != 2, outfile, "fwrite");
}
void put_16_le_seek(uint16_t value, long offset, FILE *outfile)
{
    CHECK_ERRNO(fseek(outfile, offset, SEEK_SET) != 0, "fseek");

    put_16_le(value, outfile);
}
void put_32_be(uint32_t value, FILE *outfile)
{
    unsigned char buf[4];
    write_32_be(value, buf);
    size_t bytes_written = fwrite(buf, 1, 4, outfile);
    CHECK_FILE(bytes_written != 4, outfile, "fwrite");
}
void put_32_be_seek(uint32_t value, long offset, FILE *outfile)
{
    CHECK_ERRNO(fseek(outfile, offset, SEEK_SET) != 0, "fseek");

    put_32_be(value, outfile);
}
void put_32_le(uint32_t value, FILE *outfile)
{
    unsigned char buf[4];
    write_32_le(value, buf);
    size_t bytes_written = fwrite(buf, 1, 4, outfile);
    CHECK_FILE(bytes_written != 4, outfile, "fwrite");
}
void put_32_le_seek(uint32_t value, long offset, FILE *outfile)
{
    CHECK_ERRNO(fseek(outfile, offset, SEEK_SET) != 0, "fseek");

    put_32_le(value, outfile);
}
void put_bytes(FILE *outfile, const unsigned char *buf, size_t byte_count)
{
    size_t bytes_written = fwrite(buf, 1, byte_count, outfile);
    CHECK_FILE(bytes_written != byte_count, outfile, "fwrite");
}

void put_bytes_seek(long offset, FILE *outfile, const unsigned char *buf, size_t byte_count)
{
    CHECK_ERRNO(fseek(outfile, offset, SEEK_SET) != 0, "fseek");
    put_bytes(outfile, buf, byte_count);
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

FILE * open_file_in_directory(const char *base_name, const char *dir_name, const char orig_sep, const char *file_name, const char *perms)
{
    FILE *f = NULL;
    int dir_len = 0;;
    char * full_name = NULL;
    int full_name_len = 0;

    if (dir_name)
    {
        dir_len = strlen(dir_name);
    }

    full_name = malloc(
        strlen(base_name)+1+
        dir_len+1+
        strlen(file_name)+1);

    if (!full_name)
    {
        return f;
    }

    // start with the base (from name of archive)
    strcpy(full_name, base_name);
    full_name_len = strlen(base_name);
    make_directory(base_name);

    if (dir_len)
    {
        full_name[full_name_len++] = DIRSEP;
        for (int i = 0;  i < dir_len; i++)
        {
            if (dir_name[i] == orig_sep)
            {
                // intermediate directories
                full_name[full_name_len] = '\0';
                make_directory(full_name);
                full_name[full_name_len++] = DIRSEP;
            }
            else
            {
                full_name[full_name_len++] = dir_name[i];
            }
        }

        // last directory in the chain
        full_name[full_name_len] = '\0';
        make_directory(full_name);
    }

    full_name[full_name_len++] = DIRSEP;
    strcpy(full_name+full_name_len, file_name);

    f = fopen(full_name, perms);

    free(full_name);

    return f;
}

void make_directory(const char *name)
{
#ifdef __MINGW32__
    _mkdir(name);
#else
    mkdir(name, 0755);
#endif
}

const char *strip_path(const char *path)
{
    const char * c = strrchr(path, DIRSEP);
    if (c)
    {
        // base name starts after last separator
        return c+1;
    }
    else
    {
        // no separators
        return path;
    }
}

uint8_t * get_whole_file(FILE *infile, long *file_size_p)
{
    // get input file size
    CHECK_ERRNO(-1 == fseek(infile, 0, SEEK_END), "fseek");
    const long file_size = ftell(infile);
    CHECK_ERRNO(-1 == file_size, "ftell");

    if (file_size_p)
    {
        *file_size_p = file_size;
    }

    uint8_t *indata = malloc(file_size);
    CHECK_ERRNO(!indata, "malloc");

    // dump whole file
    get_bytes_seek(0, infile, indata, file_size);

    return indata;
}

char * number_name(const char * name_head, const char * name_tail, unsigned int id, unsigned int max_id)
{
    CHECK_ERROR(id > max_id, "id > max");

    size_t numberlen = ceil(log10(max_id+1)); /* +1 to round up powers of 10 */
    size_t namelen = strlen(name_head) + numberlen + strlen(name_tail) + 1;

    char * name = malloc(namelen);
    CHECK_ERRNO(!name, "malloc");
    
    snprintf(name, namelen, "%s%0*u%s", 
        name_head, (int)numberlen, id, name_tail);

    return name;
}
