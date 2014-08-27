#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#define CHECK_ERROR(condition,message) \
do {if (condition) { \
    fprintf(stderr, "%s:%d:%s: %s\n",__FILE__,__LINE__,__func__,message); \
    exit(EXIT_FAILURE); \
}}while(0)

#define CHECK_ERRNO(condition, message) \
do {if (condition) { \
    fprintf(stderr, "%s:%d:%s:%s: ",__FILE__,__LINE__,__func__,message); \
    fflush(stderr); \
    perror(NULL); \
    exit(EXIT_FAILURE); \
}}while(0)

#define CHECK_FILE(condition,file,message) \
do {if (condition) { \
    fprintf(stderr, "%s:%d:%s:%s: ",__FILE__,__LINE__,__func__,message); \
    fflush(stderr); \
    if (feof(file)) { \
        fprintf(stderr,"unexpected EOF\n"); \
    } else { \
        perror(message); \
    } \
    exit(EXIT_FAILURE); \
}}while(0)

void dump(FILE *infile, FILE *outfile, long offset, size_t size);

void analyze(FILE *infile, long file_length);

uint32_t read_32_le(unsigned char bytes[4]);

uint16_t read_16_le(unsigned char bytes[2]);

int main(int argc, char **argv)
{
    printf("dumplarc 0.0 - extract from .la files (LARC header)\n\n");
    CHECK_ERROR(argc != 2, "Incorrect program usage\n\nusage: dumplarc input.la");

    /* open file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    analyze(infile, file_length);

    exit(EXIT_SUCCESS);
}

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

void analyze(FILE *infile, long file_length)
{
    unsigned char buf[4];

    /* read header */
    size_t bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");

    /* check header */
    const char LARC_signature[4] = "LARC"; /* intentionally unterminated */
    CHECK_ERROR(memcmp(buf, LARC_signature, sizeof(LARC_signature)),"Missing LARC signature");

    /* get file count */
    bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");
    uint32_t file_count = read_32_le(buf);
    
    printf("%d files\n", file_count);

    /* read file info */
    struct {
        char *name;
        uint32_t size;
        uint32_t offset;
    } *file_info = malloc(sizeof(file_info[0]) * file_count);
    CHECK_ERRNO(file_info == NULL, "malloc");
    for (int i=0; i<file_count; i++)
    {
        /* file name length */
        bytes_read = fread(buf, 1, 2, infile);
        CHECK_FILE(bytes_read != 2, infile, "fread");
        uint16_t name_length = read_16_le(buf);

        /* allocate for name */
        file_info[i].name = malloc(name_length + 1);
        CHECK_ERRNO(file_info[i].name == NULL, "malloc");

        /* load name */
        bytes_read = fread(file_info[i].name, 1, name_length, infile);
        CHECK_FILE(bytes_read != name_length, infile, "fread");
        file_info[i].name[name_length] = '\0';

        /* load size */
        bytes_read = fread(buf, 1, 4, infile);
        CHECK_FILE(bytes_read != 4, infile, "fread");
        file_info[i].size = read_32_le(buf);

        /* load offset */
        bytes_read = fread(buf, 1, 4, infile);
        CHECK_FILE(bytes_read != 4, infile, "fread");
        file_info[i].offset = read_32_le(buf);

    }

    long data_offset = ftell(infile);
    CHECK_ERRNO(data_offset == -1, "ftell");

    /* dump files */
    for (int i=0; i<file_count; i++)
    {
        long file_offset = data_offset + file_info[i].offset;
        printf("%02d: 0x%08" PRIx32 " 0x%08" PRIx32 " %s\n",
                i, (uint32_t)file_offset,
                file_info[i].size, file_info[i].name);
        
        FILE *outfile = fopen(file_info[i].name, "wb");
        CHECK_ERRNO(outfile == NULL, "fopen");

        dump(infile, outfile, file_offset, file_info[i].size);

        CHECK_ERRNO(fclose(outfile) == EOF, "fclose");
    }
}
