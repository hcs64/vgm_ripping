#include <stdint.h>
#include <inttypes.h>
#include "util.h"
#include "error_stuff.h"
#include "parseriff.h"

void check_riff_wave(FILE * infile, long * first_chunk_p, long * wave_end_p)
{
    long start_offset = 0;

    CHECK_ERROR(get_32_be_seek(start_offset+0, infile) != RIFF_MAGIC, "not RIFF");
    CHECK_ERROR(get_32_be_seek(start_offset+8, infile) != WAVE_MAGIC, "RIFF is not WAVE");

    long riff_size = get_32_le_seek(start_offset+4, infile) + 8;

    long file_size = get_file_size(infile);

    CHECK_ERROR(start_offset + riff_size > file_size, "RIFF truncated");

    if (first_chunk_p)
    {
        *first_chunk_p = start_offset + 12;
    }
    if (wave_end_p)
    {
        *wave_end_p = start_offset + riff_size;
    }

    return;
}

int find_chunks(FILE *infile, const long first_chunk, const long file_end, struct chunk_info * chunk_info, const int chunk_info_count, const int ignore_unknown)
{
    CHECK_ERROR (!(first_chunk >= 0 && first_chunk <= file_end), "bad range");

    long offset = first_chunk;
    for (int i = 0; i < chunk_info_count; i++)
    {
        chunk_info[i].found = 0;
        chunk_info[i].offset = -1;
        chunk_info[i].size = 0;
    }

    while (offset < file_end)
    {
        uint32_t chunk_id, chunk_size;
        int known = 0;

        if (offset + 8 > file_end) return 1;

        chunk_id = get_32_be_seek(offset+0, infile);
        chunk_size = get_32_le_seek(offset+4, infile);

        if (offset + 8 + chunk_size > file_end) return 1;

        offset += 8;

        for (int i = 0; i < chunk_info_count; i++)
        {
            if (chunk_info[i].id == chunk_id)
            {
                known = 1;

                if (chunk_info[i].found)
                {
                    if (!chunk_info[i].repeatable)
                    {
                        return 1;
                    }

                    // ignore subsequent encounters
                }
                else
                {
                    // first encounter
                    chunk_info[i].found = 1;
                    chunk_info[i].offset = offset;
                    chunk_info[i].size = chunk_size;
                }
            }
        }

        if (!known && !ignore_unknown)
        {
            return 1;
        }

        offset += chunk_size;
    }

    for (int i = 0; i < chunk_info_count; i++)
    {
        if (!chunk_info[i].found && !chunk_info[i].optional)
        {
            return 1;
        }
    }

    return 0;
}

