#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "error_stuff.h"
#include "util.h"

void process_dir_list(long dir_offset, long table_size, long data_size, FILE *infile);

int main(int argc, char **argv)
{

    /* input file name */
    const char *infile_name = NULL;
    CHECK_ERROR(argc != 2, "needs one argument: file to work on");
    infile_name = argv[1];

    /* input file */
    FILE *infile = fopen(infile_name, "rb");
    CHECK_ERRNO(NULL == infile, "input file open failed");

    CHECK_ERROR(get_32_le(infile) != 3 || get_32_le(infile) != 0 || get_32_le(infile) != 3 || get_32_le(infile) != 1 || get_32_le(infile) != 0,"whither 30310?!?");

    const uint32_t data_size = get_32_le(infile);
    printf("data size   = 0x08%" PRIx32 "\n", data_size);

    const uint32_t header_size = get_32_le(infile);
    printf("header size = 0x%08" PRIx32 "\n", header_size);

    process_dir_list(0x1C, header_size, data_size, infile);
}

void process_dir_list(long dir_offset, long table_size, long data_size, FILE *infile)
{
    long data_offset = dir_offset + table_size;

    const uint32_t dir_count = get_32_le_seek(dir_offset, infile);
    dir_offset += 4;

    // offset to info on files
    long file_offset = dir_offset;

    // advance file offset to where file info starts
    for (uint32_t i = 0; i < dir_count; i++)
    {
        CHECK_ERROR(file_offset >= data_offset, "read beyond table");
        const uint32_t name_size = get_32_le_seek(file_offset, infile);
        file_offset += 4 + name_size;
    }

    // dirs
    for (uint32_t i = 0; i < dir_count; i++)
    {
        CHECK_ERROR(dir_offset >= data_offset, "read beyond table");

        const uint32_t dir_name_size = get_32_le_seek(dir_offset, infile);
        dir_offset +=4 ;

        dump(infile, stdout, dir_offset, dir_name_size);
        printf("\n");
        dir_offset += dir_name_size;

        if (file_offset == data_offset) break;

        const uint32_t file_count = get_32_le_seek(file_offset, infile);
        file_offset += 4;

        // contents of dir
        for (uint32_t j = 0; j < file_count; j++)
        {
            CHECK_ERROR(file_offset >= data_offset, "read beyond table");
            const uint32_t file_name_size = get_32_le_seek(file_offset, infile);
            file_offset += 4;

            printf("  ");
            dump(infile, stdout, file_offset, file_name_size);
            file_offset += file_name_size;

            CHECK_ERROR(get_32_le_seek(file_offset, infile) != 0, "zernooo");
            const uint32_t offset = get_32_le_seek(file_offset+4, infile)+data_offset;
            const uint32_t size = get_32_le_seek(file_offset+8, infile);
            printf(": offset 0x%"PRIx32" size 0x%"PRIx32"\n",offset,size);
            {
                unsigned char namebuf[file_name_size+1];
                get_bytes_seek(file_offset-file_name_size,infile,namebuf,file_name_size);
                namebuf[file_name_size-1]='\0';
                FILE *outfile = fopen((char*)namebuf,"wb");
                CHECK_ERRNO(outfile == NULL,"fopen");
                dump(infile, outfile, offset+0x38, size-0x38);
                CHECK_ERRNO(fclose(outfile) == EOF,"fclose");
            }
            file_offset += 0xc;
        }
    }
}
