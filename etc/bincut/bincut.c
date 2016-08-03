#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "error_stuff.h"
#include "util.h"

static inline int hex2int(char c)
{
    CHECK_ERROR(!isxdigit(c), "nonhex character");
    if (isdigit(c)) return c-'0';
    else return tolower(c)-'a'+10;
}

void parse_range(char *str, long *startp, long *endp, long length)
{
    int l = strlen(str);
    int doing_start = 1;    /* and if not doing start, doing end */
    long start = 0, end = 0;

    CHECK_ERROR(l == 0, "zero length range string");
    for (int i=0;i<l;i++)
    {
        if (doing_start)
        {
            if (str[i] == '-')
            {
                doing_start = 0;
                continue;
            }
            start = start*16 + hex2int(str[i]);
        }
        else
        {
            end = end*16 + hex2int(str[i]);
        }
    }

    CHECK_ERROR(doing_start, "incomplete range");
    if (str[l-1] == '-')
    {
        /* open ended range is to file end */
        end = length;
    }
    CHECK_ERROR(start < 0 || end < 0 || end <= start, "invalid range");

    *startp = start;
    *endp = end;
}

void parse_repeat_stride(char *repeat_str, char *stride_str, long *repeatp, long *stridep) {
    int l = strlen(repeat_str);
    long repeat = 0;

    CHECK_ERROR(l == 0, "zero length repeat string");
    for (int i=0;i<l;i++)
    {
        repeat = repeat*16 + hex2int(repeat_str[i]);
    }
    *repeatp = repeat;

    l = strlen(stride_str);
    long stride = 0;

    CHECK_ERROR(l == 0, "zero length stride string");
    for (int i=0;i<l;i++)
    {
        stride = stride*16 + hex2int(stride_str[i]);
    }
    *stridep = stride;
}

int main(int argc, char **argv)
{
    long start_off, end_off;
    FILE *infile, *outfile;
    long repeat = 1, stride = 0;

    if (argc != 4 && argc != 6) {
        fprintf(stderr,"usage: bincut infile outfile range [repeat stride]\n"); exit(1);
    }

    /* open input file */
    infile = fopen(argv[1], "rb");
    CHECK_ERRNO(infile == NULL, "fopen of input file");

    outfile = fopen(argv[2], "wb");
    CHECK_ERRNO(outfile == NULL, "fopen of output file");

    CHECK_ERRNO(fseek(infile, 0, SEEK_END) == -1, "fseek");
    long infile_size = ftell(infile);
    CHECK_ERRNO(infile_size == -1, "ftell");

    parse_range(argv[3], &start_off, &end_off, infile_size);

    if (argc == 6) {
        parse_repeat_stride(argv[4], argv[5], &repeat, &stride);
    }

    printf("(range=%lx to %lx)\n",start_off,end_off);

    if (repeat == 0) {
        if (stride > 0) {
            repeat = (infile_size-start_off-(end_off-start_off))/stride+1;
        }
    }

    for (long i = 0; i < repeat; i ++)
    {
        dump(infile, outfile, start_off+i*stride, end_off-start_off);
    }

    CHECK_ERRNO(fclose(infile), "fclose of input file");
    CHECK_ERRNO(fclose(outfile), "fclose of output file");
}
