#include <stdio.h>

#include "utf_tab.h"
#include "error_stuff.h"
#include "util.h"

void analyze(reader_t *infile, long offset, long file_length);

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
    reader_t *infile = open_reader_file(argv[1]);
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    long file_length = reader_length(infile);

    analyze(infile, offset, file_length);

    close_reader(infile);

    exit(EXIT_SUCCESS);
}

void analyze(reader_t *infile, long offset, long file_length)
{
    int indent = 0;

    reader_t *crypt_infile = open_reader_crypt(infile, offset, 0x5f, 0x15);

    analyze_utf(crypt_infile, offset, indent, 1, NULL);

    close_reader(crypt_infile);
}
