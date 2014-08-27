#include <stdio.h>
#include "error_stuff.h"
#include "util.h"

// SwapByteBits from GHIII FSB Decryptor v1.0 by Invo
unsigned char SwapByteBits(unsigned char cInput)
{
   unsigned char nResult=0;

   for(int i=0; i<8; i++)
   {
      nResult = nResult << 1;
      nResult |= (cInput & 1);
      cInput = cInput >> 1;
   }

   return (nResult);
}

void dump_xor(FILE *infile, FILE *outfile, long offset, size_t size, unsigned char const * const key, size_t key_size)
{
    unsigned char buf[0x800];
    int key_index = 0;
    uint_fast8_t swap_table[0x100];

    for (int i=0;i<0x100;i++)
        swap_table[i] = SwapByteBits(i);

    CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

    while (size > 0)
    {
        size_t bytes_to_copy = sizeof(buf);
        if (bytes_to_copy > size) bytes_to_copy = size;

        size_t bytes_read = fread(buf, 1, bytes_to_copy, infile);
        CHECK_FILE(bytes_read != bytes_to_copy, infile, "fread");

        for (int i = 0; i < bytes_to_copy; i++)
        {
            buf[i] = swap_table[buf[i]] ^ key[key_index];
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
    unsigned char *key = NULL;
    size_t key_length = 0;

    printf("decfsb 0.2\n");
    if (argc != 4)
    {
        if (argc < 5 || strcmp(argv[3],"-x"))
        {
            printf("usage: decfsb infile outfile [key | -x 11 2A BB ...]\n");
            exit(EXIT_FAILURE);
        }

        key_length = argc-4;
        key = malloc(key_length*sizeof(unsigned char));
        CHECK_ERRNO(!key, "malloc");
        for (int i=4; i < argc; i++)
        {
            unsigned int tempkey;
            sscanf(argv[i], "%x", &tempkey);
            key[i-4] = tempkey;
        }
    }
    else
    {
        key = (unsigned char*)argv[3];
        key_length = strlen(argv[3]);
    }

    FILE *infile = fopen(argv[1],"rb");
    CHECK_ERRNO(!infile, "error opening infile");
    FILE *outfile = fopen(argv[2],"wb");
    CHECK_ERRNO(!outfile, "error opening outfile");

    CHECK_ERRNO(-1 == fseek(infile, 0, SEEK_END), "fseek");
    const long file_size = ftell(infile);
    CHECK_ERRNO(-1 == file_size, "ftell");

    dump_xor(infile, outfile, 0, file_size, key, key_length);

    CHECK_ERRNO(EOF == fclose(infile), "fclose infile");
    CHECK_ERRNO(EOF == fclose(outfile), "fclose outfile");

    if (argc != 4)
    {
        free(key);
    }
}
