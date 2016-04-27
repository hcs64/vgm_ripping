#include <stdio.h>
#include "error_stuff.h"
#include "util.h"

void dump_xor(FILE *infile, FILE *outfile, long offset, size_t size, unsigned char const * const key, size_t key_size)
{
    unsigned char buf[0x800];
    int key_index = size % key_size; // key is offset by this somehow...

    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    while (size > 0)
    {
        size_t bytes_to_copy = sizeof(buf);
        if (bytes_to_copy > size) bytes_to_copy = size;

        size_t bytes_read = fread(buf, 1, bytes_to_copy, infile);
        CHECK_FILE(bytes_read != bytes_to_copy, infile, "fread");

        for (int i = 0; i < bytes_to_copy; i++)
        {
            buf[i] = buf[i] ^ key[key_index];
            key_index++;
            if (key_index == key_size) key_index = 0;
        }

        size_t bytes_written = fwrite(buf, 1, bytes_to_copy, outfile);
        CHECK_FILE(bytes_written != bytes_to_copy, outfile, "fwrite");

        size -= bytes_to_copy;
    }
}

int main(int argc, char **argv)
{
    CHECK_ERROR(argc != 4, "3 args: infile outfile key");
    FILE *infile = fopen(argv[1],"rb");
    CHECK_ERRNO(!infile, "error opening infile");
    FILE *outfile = fopen(argv[2],"wb");
    CHECK_ERRNO(!outfile, "error opening outfile");

    CHECK_ERRNO(-1 == fseek(infile, 0, SEEK_END), "fseek");
    const long file_size = ftell(infile);
    CHECK_ERRNO(-1 == file_size, "ftell");

    printf("%s file size: %lx\n", argv[1], (unsigned long)file_size);

    unsigned char *key = (unsigned char*)argv[3];
    size_t key_length = strlen(argv[3]);

    dump_xor(infile, outfile, 0, file_size, key, key_length);

    CHECK_ERRNO(EOF == fclose(infile), "fclose infile");
    CHECK_ERRNO(EOF == fclose(outfile), "fclose outfile");
}
