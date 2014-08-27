#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "xma_rebuild.h"
#include "fsbext.h"
#include "util.h"
#include "error_stuff.h"

/* extract fsb substreams */
/* from fsb_mpeg 0.12 */

static long gBodyPadding = 0;

const char fsb3headmagic[4]="FSB3"; /* not terminated */
const char fsb4headmagic[4]="FSB4"; /* not terminated */

const int fsb3headsize = 0x18;
const int fsb4headsize = 0x30;
const int maxheadsize = 0x30;

enum fsb_type_t {
    fsb3, fsb4
};

int try_multistream_fsb(const uint8_t *infile, long file_size, subfile_callback_t *cb, void *cbv)
{
    int32_t stream_count;
    int32_t table_size;
    int32_t body_size;
    int32_t header_size;
    enum fsb_type_t fsb_type;

    /* read header */
    CHECK_ERROR(file_size < 4, "too small to be FSB");

    {
        if (!memcmp(&infile[0],fsb3headmagic,4))
        {
            printf("Type: FSB3\n");
            header_size = fsb3headsize;
            fsb_type = fsb3;
        }
        else if (!memcmp(&infile[0],fsb4headmagic,4))
        {
            printf("Type: FSB4\n");
            header_size = fsb4headsize;
            fsb_type = fsb4;
        }
        else
        {
            /* couldn't find a valid multistream fsb to unpack */
            return 1;
        }

        if (header_size > file_size)
        {
            return 1;
        }

        /* read the rest of the header */
        stream_count = read_32_le(&infile[4]);
        if (stream_count <= 0)
        {
            return 1;
        }

        table_size = read_32_le(&infile[8]);
        if (table_size <= 0)
        {
            return 1;
        }
        body_size = read_32_le(&infile[12]);
        if (body_size <= 0)
        {
            return 1;
        }

        printf("Header: 0x%" PRIx32 " bytes\n", (uint32_t)header_size);
        printf("Table:  0x%" PRIx32 " bytes\n", (uint32_t)table_size);
        printf("Body:   0x%" PRIx32 " bytes\n", (uint32_t)body_size);
        printf("------------------\n");

        uint64_t total_size = (uint64_t)header_size +
                (uint64_t)table_size +
                (uint64_t)body_size;
        printf("Total:  0x%lx bytes\n", (unsigned long)total_size);
        printf("File:   0x%lx bytes\n", (unsigned long)file_size);

        if (file_size < total_size)
        {
            return 1;
        }

        printf("%" PRId32 " subfile%s\n\n", stream_count, (1==stream_count?"":"s"));
    }

    /* unpack each subfile */
    {
        long table_offset = header_size;
        long body_offset = header_size + table_size;

        for (int i = 0; i < stream_count; i++) {
            int16_t entry_size;
            int16_t padding_size, body_padding_size;
            int32_t entry_file_size;
            char name_buf[0x1e + 1 + 1];
            char *stream_name;
            const int entry_min_size = 0x40;

            entry_size = read_16_le(&infile[table_offset+0x00]);
            if (entry_size < entry_min_size)
            {
                return 1;
            }
            padding_size = 0x10 - (header_size + entry_size) % 0x10;

            entry_file_size = read_32_le(&infile[table_offset+0x24]);

            if (gBodyPadding != 0)
            {
                body_padding_size = 
                    (entry_file_size + gBodyPadding - 1) / gBodyPadding * gBodyPadding -
                    entry_file_size;
            }
            else
            {
                body_padding_size = 0;
            }

            /* copy the name somewhere we can play with it */
            memset(name_buf, 0, sizeof(name_buf));
            memcpy(name_buf, &infile[table_offset+0x02], 0x1e);
            name_buf[strlen(name_buf)]='_';

#if 0
            /* generate a unique namebase by prepending subfile number */
            {
                size_t numberlen = ceil(log10(stream_count+2)); /* +1 because we add 1, +1 to round up even counts */
            /* "number_name\0" */
                size_t namelen = numberlen + 1 + strlen(name_buf) + 1;
                name_base = malloc(namelen);
                CHECK_ERRNO(!name_base, "malloc");

                snprintf(name_base, namelen, "%0*u_%s", 
                    (int)numberlen, (unsigned int)(i+1), name_buf);
            }
#endif
            stream_name = number_name(name_buf, "_", i+1, stream_count);

            printf("%d: %s"
                       " header 0x%02" PRIx32
                       " entry 0x%04" PRIx16 " (+0x%04" PRIx16 " padding)"
                       " body 0x%08" PRIx32 " (+0x%04" PRIx16 " padding)"
                       " (0x%08" PRIx32 ")\n",
                       i,
                       name_buf,
                       (uint32_t)header_size,
                       (uint16_t)entry_size,
                       (uint16_t)padding_size,
                       (uint32_t)entry_file_size,
                       (uint32_t)body_padding_size,
                       (uint32_t)body_offset);

            /* get sample rate */
            uint32_t srate = read_32_le(&infile[table_offset+0x34]);

            /* get channel count */
            uint16_t channels = read_32_le(&infile[table_offset+0x3e]);
            int substreams = (channels + 1) / 2;

            /* get sample count */
            uint32_t samples = read_32_le(&infile[table_offset+0x20]);

            printf("%"PRIu32" Hz %"PRIu16" channel%s (%d streams), %"PRIu32" samples\n\n",
                    srate, channels, (1==channels?"":"s"), substreams, samples);

            // invoke callback
            if (0 != cb(infile + body_offset, entry_file_size, substreams, NULL,
                        samples, srate, default_block_size,
                        0, 0, // TODO: collect loop info
                        stream_name, cbv))
            {
                free(stream_name);
                return 1;
            }

            free(stream_name);

            /* onward! */
            table_offset += entry_size;
            body_offset += entry_file_size + body_padding_size;
        }
    }

    return 0;
}

