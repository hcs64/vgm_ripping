#include <stdio.h>
#include <inttypes.h>

int main(int argc, char **argv)
{
    if (2 != argc)
    {
        fprintf(stderr,"usage: nibble_reverse file.ima\n");
        return 1;
    }

    FILE *infile = fopen(argv[1], "r+b");
    if (!infile)
    {
        perror("fopen");
        return 1;
    }


    if (-1 == fseek(infile, 0, SEEK_END))
    {
        perror("fseek");
        return 1;
    }
    long filesize = ftell(infile);
    if (-1 == filesize)
    {
        perror("ftell");
        return 1;
    }

    uint8_t buf[0x200];

    long offset = 0;
    while (offset < filesize)
    {
        long bytes_to_do = sizeof(buf);
        if (bytes_to_do > filesize - offset)
        {
            bytes_to_do = filesize - offset;
        }
        if (-1 == fseek(infile, offset, SEEK_SET))
        {
            perror("fseek");
        }
        if (bytes_to_do != fread(buf, 1, bytes_to_do, infile))
        {
            perror("fread");
            return 1;
        }

        for (long i = 0; i < bytes_to_do; i++)
        {
            buf[i] = (buf[i]>>4)|(buf[i]<<4);
        }

        if (-1 == fseek(infile, offset, SEEK_SET))
        {
            perror("fseek");
        }
        if (bytes_to_do != fwrite(buf, 1, bytes_to_do, infile))
        {
            perror("fwrite");
            return 1;
        }

        offset += bytes_to_do;
    }

    if (EOF == fclose(infile))
    {
        perror("fclose");
    }
}
