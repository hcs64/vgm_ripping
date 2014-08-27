#include <stdint.h>
#include <inttypes.h>
#include "util.h"
#include "error_stuff.h"
#include "fsbext.h"
#include "riffext.h"
#include "bnkext.h"
#include "xma_rebuild.h"

// wwise bank

struct chunk_info
{
    // initial values
    uint32_t id;
    int optional;
    int repeatable; // only first will be filled in

    // filled in by find_chunks
    int found;
    long offset;
    uint32_t size;
};

static int find_chunks(const uint8_t *infile, long file_size, struct chunk_info * chunk_info, int chunk_info_count, int ignore_unknown)
{
    long offset = 0;
    for (int i = 0; i < chunk_info_count; i++)
    {
        chunk_info[i].found = 0;
        chunk_info[i].offset = -1;
        chunk_info[i].size = 0;
    }

    while (offset < file_size)
    {
        uint32_t chunk_id, chunk_size;
        int known = 0;

        if (offset + 8 > file_size) return 1;

        chunk_id = read_32_be(&infile[offset+0]);
        chunk_size = read_32_be(&infile[offset+4]);

        if (offset + 8 + chunk_size > file_size) return 1;

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

static int try_ww_xma_riff(const uint8_t *infile, long file_size, const char *stream_name, subfile_callback_t *cb, void *cbv);

int try_wwbnk(const uint8_t *infile, long file_size, subfile_callback_t *cb, void *cbv)
{
    struct chunk_info chunks[] =
    {
        { .id = 0x424B4844 }, // 0 BKHD
        { .id = 0x44494458 }, // 1 DIDX
        { .id = 0x44415441 }, // 2 DATA
        { .id = 0x48495243 }, // 3 HIRC
        { .id = 0x53544944 }, // 4 STID
    };

    const int DIDX_IDX = 1;
    const int DATA_IDX = 2;

    if (0 != find_chunks(infile, file_size, chunks, sizeof(chunks)/sizeof(chunks[0]), 0))
    {
        return 1;
    }
        
    // parse DIDX
    {
        const int DIDX_ENTRY_SIZE = 0xC;

        long offset = chunks[DIDX_IDX].offset;
        long didx_size = chunks[DIDX_IDX].size;
        long data_offset = chunks[DATA_IDX].offset;
        long data_size = chunks[DATA_IDX].size;

        if (didx_size % DIDX_ENTRY_SIZE != 0) return 1;

        int file_count = didx_size / DIDX_ENTRY_SIZE;

        for (int idx = 0; idx < file_count; idx ++)
        {
            uint32_t crc = read_32_be(&infile[offset + 0]);
            long subfile_offset = data_offset + read_32_be(&infile[offset + 4]);
            long subfile_size = read_32_be(&infile[offset + 8]);

            if (subfile_offset + subfile_size > data_offset + data_size)
            {
                return 1;
            }

            char crc_string[11+1];
            memset(crc_string, 0, sizeof(crc_string));
            snprintf(crc_string, sizeof(crc_string), "%"PRIu32"_", crc);
            char *file_name = number_name(crc_string, "_", idx, file_count);

            int rc = try_ww_xma_riff(infile + subfile_offset, subfile_size, file_name, cb, cbv);

            if (rc == 2)
            {
                printf("subfile %"PRIu32" not XMA\n", crc);
                cb(infile + subfile_offset, subfile_size, 0, NULL, 0, 0, 0, 0, 0, file_name, cbv);
            }

            free(file_name);
            file_name = NULL;

            if (rc == 1)
            {
                return 1;
            }

            offset += DIDX_ENTRY_SIZE;
        }
    }

    return 0;
}

static int try_ww_xma_riff(const uint8_t *infile, long file_size, const char *stream_name, subfile_callback_t *cb, void *cbv)
{
    uint32_t (*read_32)(const unsigned char []) = NULL;
    uint16_t (*read_16)(const unsigned char []) = NULL;

    if (file_size < 12) return 1;

    if (!memcmp(infile, "RIFX", 4))
    {
        read_32 = read_32_be;
        read_16 = read_16_be;
    }
    else if (!memcmp(infile, "RIFF", 4))
    {
        read_32 = read_32_le;
        read_16 = read_16_le;
    }
    else
    {
        return 1;
    }

    if (read_32(&infile[4]) + 8 != file_size)
    {
        // some RIFX have little endian sizes without the -8
        if (read_32_le(&infile[4]) != file_size)
        {
            printf("nope\n");
            return 1;
        }
    }

    if (memcmp(&infile[8], "WAVE", 4))
    {
        return 1;
    }

    struct chunk_info chunks[] =
    {
        { .id = 0x666d7420 }, // 0 fmt
        { .id = 0x64617461 }, // 1 data
        { .id = 0x584d4163, .optional = 1 }, // 2 XMAc
        { .id = 0x7365656b, .optional = 1 }, // 3 seek
    };

    const int FMT_IDX = 0;
    const int DATA_IDX = 1;
    const int XMAC_IDX = 2;

    if (0 != find_chunks(infile + 12, file_size - 12, chunks, sizeof(chunks)/sizeof(chunks[0]), 1))
    {
        return 1;
    }

    if (chunks[XMAC_IDX].offset == -1)
    {
        return 2;
    }

    long fmt_offset = 12 + chunks[FMT_IDX].offset;
    uint16_t codec = read_16(&infile[fmt_offset+0]);
    uint16_t channels = read_16(&infile[fmt_offset+2]);
    uint32_t sample_rate = read_32(&infile[fmt_offset+4]);
    uint16_t fmt_extra = read_16(&infile[fmt_offset+0x10]);

    if (codec != 0x166)
    {
        return 2;
    }

    if (channels > 2)
    {
        printf("no support for %d channel(s)\n", channels);
        return 1;
    }

    if (fmt_extra != 0x22)
    {
        printf("expected 0x22 extra, got 0x%02"PRIx16"\n", fmt_extra);
        return 1;
    }

    uint32_t sample_count = read_32(&infile[fmt_offset + 0x18]);
    uint32_t block_size = read_32(&infile[fmt_offset + 0x1C]);

    long data_offset = 12 + chunks[DATA_IDX].offset;
    long data_size = chunks[DATA_IDX].size;

    int * stream_channels = malloc(sizeof(int));
    stream_channels[0] = channels;

    if (0 != cb(infile + data_offset, data_size, 1, stream_channels,
            sample_count, sample_rate, block_size,
            0, 0, // TODO: collect loop info
            stream_name, cbv))
    {
        return 1;
    }

    return 0;
}
