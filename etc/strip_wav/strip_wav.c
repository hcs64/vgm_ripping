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

void dump(FILE *infile, FILE *outfile, long offset, size_t size);
int check_0(FILE *infile, long offset, size_t size);
char *backup_name = NULL;
void backup(FILE *infile, long file_length, const char *name);

void update(FILE *infile, long old_length, long new_length);

uint32_t read_32_le(unsigned char bytes[4]);

uint16_t read_16_le(unsigned char bytes[2]);

int main(int argc, char **argv)
{
    CHECK_ERROR(argc != 2 && argc != 3, "strip_wav 0.1 - remove bytes from the end of a WAV\n\nusage: strip_wav input.wav [bytes to remove]\nDefault is to remove 44100*4*10 bytes (10 seconds of 44100Hz 16-bit stereo PCM\n");

    long bytes = 44100*4*10;
    if (argc == 3)
    {
        bytes = atol(argv[2]);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "r+b");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    long new_length = file_length - bytes;
    if (file_length < bytes)
        new_length = file_length;

    if (!check_0(infile, file_length-new_length, new_length-file_length))
    {
        backup(infile, file_length, argv[1]);
    }

    update(infile, file_length, new_length);

    CHECK_ERRNO(fclose(infile) != 0, "fclose");

    /* truncate */
    int fd = open(argv[1], O_WRONLY);
    CHECK_ERRNO( fd == -1, "open" );

    CHECK_ERRNO(ftruncate(fd, new_length) != 0, "truncate");

    CHECK_ERRNO( fd == -1, "close" );

    if (!backup_name)
        printf("add_silence %s %ld\n", argv[1], file_length - new_length);
    else
    {
        printf("copy %s %s\n", backup_name, argv[1]);
        free(backup_name);
    }

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

const unsigned char zerobuf[0x800];

int check_0(FILE *infile, long offset, size_t size)
{
    unsigned char buf[0x800];

    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    while (size > 0)
    {
        size_t bytes_to_check = sizeof(buf);
        if (bytes_to_check > size) bytes_to_check = size;

        size_t bytes_read = fread(buf, 1, bytes_to_check, infile);
        CHECK_FILE(bytes_read != bytes_to_check, infile, "fread");

        if (memcmp(buf,zerobuf,bytes_to_check))
        {
            return 0;
        }

        size -= bytes_to_check;
    }

    return 1;
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

void backup(FILE *infile, long file_length, const char *name)
{
    const char bak_ext[] = ".bak";
    size_t namelen = strlen(name);
    size_t bak_ext_len = strlen(bak_ext);

    if (backup_name) free(backup_name);

    backup_name = malloc(namelen+bak_ext_len+1);
    CHECK_ERRNO(!backup_name, "malloc");

    memcpy(backup_name, name, namelen);
    memcpy(backup_name+namelen, bak_ext, bak_ext_len+1);

    FILE *outfile = fopen(backup_name, "wb");
    CHECK_ERRNO(!outfile, "fopen");

    dump(infile, outfile, 0, file_length);
    
    CHECK_ERRNO(fclose(outfile) != 0, "fclose");
}
