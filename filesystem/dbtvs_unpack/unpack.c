// unpack PACKFILE.BIN from "Dragon Ball Tag VS" / "Dragon Ball Z Tenkaichi Tag Team

#include "util.h"

uint32_t round_up(uint32_t offset, uint32_t page_size)
{
    return (offset+page_size-1)/page_size*page_size;
}

int main(int argc, char ** argv)
{
    const uint32_t key = 0x4B636150;  // "PacK", little endian

    char * infile_name = 0;
    char * outdir_name = "output";
    reader_t * infile = 0;

    if (argc < 2 || argc > 3)
    {
        fprintf(stderr, "usage: unpack PACKFILE.BIN [output directory]\n");
        exit(EXIT_FAILURE);
    }

    infile_name = argv[1];
    if (argc == 3)
    {
        outdir_name = argv[2];
    }

    infile = open_reader_file(infile_name);
    CHECK_ERRNO(!infile, "error opening infile");

    {
        uint32_t magic = get_32_le(infile);
        CHECK_ERROR(magic != key, "bad magic");
    }

    {
        uint32_t file_count = get_32_le(infile);
        const long table_start = 0x10;
        const long body_start = round_up(table_start + file_count * 0x10,0x1000);
        uint32_t running_offset = body_start;

        for (uint32_t file_idx = 0; file_idx < file_count; file_idx ++)
        {
            struct {
                uint32_t offset, size, comp_size;
                uint32_t flags;
            } file;

            file.offset = (get_32_le_seek(table_start+file_idx*0x10, infile)^key) * 0x800 + body_start;
            file.size = get_32_le(infile)^file_idx;
            file.comp_size = get_32_le(infile)^key;
            file.flags = get_32_le(infile)^file_idx;

            CHECK_ERROR(file.offset != running_offset, "offset calc error");

            printf("%8"PRIx32":", file_idx);

            if (file.flags & 0x80000000)
            {
                printf("\t-%08"PRIx32, ~file.flags);
            }
            else
            {
                printf("\t %08"PRIx32, file.flags);
            }

            printf("\t%08"PRIx32"\t%08"PRIx32, file.offset, file.size);

            if (file.comp_size > 0)
            {
                printf(" (%08"PRIx32")", file.comp_size);

            }

            printf("\n");

            // dump!

            if (file.size > 0)
            {
                char outfile_name[8+4+1];
                FILE * outfile = 0;
                long outfile_size;

                if (file.comp_size > 0)
                {
                    snprintf(outfile_name, sizeof(outfile_name), "%08"PRIx32".gz", file_idx);
                    outfile_size = file.comp_size;
                }
                else
                {
                    snprintf(outfile_name, sizeof(outfile_name), "%08"PRIx32, file_idx);
                    outfile_size = file.size;
                }

                outfile = open_file_in_directory(outdir_name, NULL, '/', outfile_name, "wb");
                CHECK_ERRNO(!outfile, "error opening output");
                dump(infile, outfile, file.offset, outfile_size);
                CHECK_ERRNO(fclose(outfile) == EOF, "output error");

                running_offset += round_up(outfile_size, 0x800);
            }
        }
    } // end for file_idx
}
