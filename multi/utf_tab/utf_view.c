#include <stdio.h>

#include "utf_tab.h"
#include "error_stuff.h"
#include "util.h"

void analyze(FILE *infile, long offset, long file_length);

int main(int argc, char **argv)
{
    printf("utf_view " VERSION "\n\n");
    CHECK_ERROR(argc != 2 && argc != 3, "Incorrect program usage\n\nusage: utf_tab file [offset]");

    long offset = 0;
    if (argc == 3)
    {
        offset = read_long(argv[2]);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    analyze(infile, offset, file_length);

    exit(EXIT_SUCCESS);
}

void analyze(FILE *infile, long offset, long file_length)
{
    int indent = 0;

    analyze_utf(infile, offset, indent, 1, NULL);
}
