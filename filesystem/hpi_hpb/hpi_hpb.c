#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stddef.h>

#include "util.h"
#include "error_stuff.h"

int main(int argc, char ** argv) {
    if (argc != 3)
    {
        fprintf(stderr, "usage: hpi_hpb MORI5.HPI MORI5.HPB\n");
        return -1;
    }

    FILE * indexfile = fopen(argv[1], "rb");
    FILE * datafile  = fopen(argv[2], "rb");

    char * index_basename = copy_string(strip_path(argv[1]));
    {
        char * index_basename_ext = strrchr(index_basename, '.');
        CHECK_ERROR(index_basename_ext == NULL, "no extension on index?");
        *index_basename_ext = '\0';
    }

    CHECK_ERROR(get_32_be_seek(0, indexfile) != UINT32_C(0x48504948), "HPIH header missing");

    long index_file_size;
    uint8_t * const index_buf = get_whole_file(indexfile, &index_file_size);
    fclose(indexfile);
    indexfile = NULL;

    EXPECT_32_BE(index_buf+0x00, 0x48504948, "HPIH");
    EXPECT_32_LE(index_buf+0x04, 0,        "0 at 0x04");
    EXPECT_32_LE(index_buf+0x08, 0x10,     "0x10 at 0x08");
    EXPECT_32_LE(index_buf+0x0C, 0,        "0 at 0x0C");
    EXPECT_16_LE(index_buf+0x10, 0,        "0 at 0x10");
    EXPECT_16_LE(index_buf+0x12, 0x1000,   "0x1000 at 0x12");
    const uint32_t index_entries = read_16_le(index_buf + 0x12);
    const uint32_t index_size = index_entries * 4;
    const uint32_t entries = read_32_le(index_buf + 0x14);

    const uint32_t index_offset   = 0x18;
    const uint32_t entry_offset   = index_offset + index_size;
    const uint32_t strings_offset = entry_offset + entries * 0x10;

    CHECK_ERROR(strings_offset > index_file_size, "file is too small");

    //printf("Index:   %8"PRIx32"\n", index_offset);
    printf("Entries: %8"PRIx32"\n", entry_offset);
    printf("Strings: %8"PRIx32"\n", strings_offset);

    printf("  id    offset     size    type?    name\n");
    printf("----------------------------------------\n");
#if 0
    for (uint16_t ei = 0; ei < index_entries; ei ++)
    {
        const uint8_t * index_entry = index_buf + index_offset + ei*4;
        const uint16_t first_entry = read_16_le(index_entry + 0);
        const uint16_t last_entry =
            first_entry + read_16_le(index_entry + 4) - 1;

        CHECK_ERROR(last_entry >= entries, "entry out of range");

        printf("group %u\n", (unsigned int)ei);
#endif

        for (uint16_t j = 0; j < entries; j++)
        {
            const uint8_t * entry = index_buf + entry_offset + j*0x10;
            const uint32_t file_name_offset = strings_offset + read_32_le(entry+0x0);
            CHECK_ERROR(file_name_offset >= index_file_size, "string idx out of range"); 
            const uint8_t * file_name = index_buf + file_name_offset;
            const uint32_t hpb_offset = read_32_le(entry+0x4);
            const uint32_t hpb_size   = read_32_le(entry+0x8);
            const uint32_t hpb_type   = read_32_le(entry+0xc);

            printf("%4"PRIx16": %8"PRIx32" %8"PRIx32" %8"PRIx32"    %.*s\n",
                j, hpb_offset, hpb_size, hpb_type,
                (int)(index_file_size-file_name_offset), file_name);

            
            char * name_copy = copy_string((char*)file_name);
            char * dir_name = name_copy;
            char * file_name_base = strrchr(name_copy, '/');
            if (file_name_base == NULL)
            {
                file_name_base = name_copy;
                dir_name = "";
            }
            else
            {
                file_name_base[0] = '\0';
                file_name_base ++;
            }
            FILE * outfile = open_file_in_directory(index_basename, dir_name, '/', file_name_base, "wb");

            free(name_copy);

            dump(datafile, outfile, hpb_offset, hpb_size);

            CHECK_ERRNO(fclose(outfile) == EOF, "fclose output");
        }

#if 0
        printf("\n");
    }
#endif

    free(index_basename);

    fclose(datafile);
}
