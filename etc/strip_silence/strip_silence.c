#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

long compute(FILE *infile, long file_length, long seconds);

void update(FILE *infile, long old_length, long new_length);

uint32_t read_32_le(unsigned char bytes[4]);

uint16_t read_16_le(unsigned char bytes[2]);

int main(int argc, char **argv)
{
    CHECK_ERROR(argc != 2 && argc != 3, "strip_silence 0.1 - remove silent frames from the end of 44.1khz PCM WAV\n\nIncorrect program usage\n\nusage: strip_silence input.wav [seconds to leave]\nDefault is to leave 1 second");

    long seconds = 1;
    if (argc == 3)
    {
        seconds = atol(argv[2]);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "r+b");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    long new_length = compute(infile, file_length, seconds);
    update(infile, file_length, new_length);

    CHECK_ERRNO(fclose(infile) != 0, "fclose");

    /* truncate */
    int fd = open(argv[1], O_WRONLY);
    CHECK_ERRNO( fd == -1, "open" );

    CHECK_ERRNO(ftruncate(fd, new_length) != 0, "truncate");

    CHECK_ERRNO( fd == -1, "close" );

    printf("add_silence %s %ld\n", argv[1], file_length - new_length);

    exit(EXIT_SUCCESS);
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
void write_32_le(unsigned char bytes[4], uint32_t v)
{
    for (int i=0; i<4; i++)
    {
        bytes[i] = v & 0xff;
        v >>= 8;
    }
}

long compute(FILE *infile, long file_length, long seconds)
{
    unsigned char buf[4];
    size_t bytes_read;

    long offset = file_length-4;
    /* read backwards to first non-silent frame */
    while (offset > 0)
    {
        CHECK_ERRNO(fseek(infile, offset , SEEK_SET) != 0, "fseek");

        bytes_read = fread(buf, 1, 4, infile);
        CHECK_FILE(bytes_read != 4, infile, "fread");

        if (read_16_le(buf) != 0 || read_16_le(buf+2) != 0)
        {
            break;
        }

        offset -= 4;
    }

    long new_length = offset + 4 + 44100*4*seconds;
    if (file_length < new_length)
        new_length = file_length;

    return new_length;
}

void update(FILE *infile, long old_length, long new_length)
{
    unsigned char buf[4];
    size_t bytes_read;
    size_t bytes_written;

    /* check old RIFF length */
    CHECK_ERRNO(fseek(infile, 0x04, SEEK_SET) != 0, "fseek");

    bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");

    CHECK_ERROR(read_32_le(buf)+8 != old_length, "RIFF size doesn't check out (extra chunks?)\n");

    /* check old data length */
    CHECK_ERRNO(fseek(infile, 0x28, SEEK_SET) != 0, "fseek");

    bytes_read = fread(buf, 1, 4, infile);
    CHECK_FILE(bytes_read != 4, infile, "fread");
    CHECK_ERROR(read_32_le(buf)+0x2c != old_length, "data size doesn't check out (extra chunks?)\n");


    /* update RIFF length */
    CHECK_ERRNO(fseek(infile, 0x04, SEEK_SET) != 0, "fseek");

    write_32_le(buf, new_length-8);
    bytes_written = fwrite(buf, 1, 4, infile);
    CHECK_FILE(bytes_written != 4, infile, "fwrite");

    /* update data length */
    CHECK_ERRNO(fseek(infile, 0x28, SEEK_SET) != 0, "fseek");

    write_32_le(buf, new_length-0x2c);
    bytes_written = fwrite(buf, 1, 4, infile);
    CHECK_FILE(bytes_written != 4, infile, "fwrite");
}
