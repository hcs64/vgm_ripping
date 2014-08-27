#include <stdint.h>
#include "fsbext.h"
#include "util.h"
#include "error_stuff.h"
#include "xma_rebuild.h"

// process the data chunk in a RIFF
// based on vgmstream r918

// returns 0 if all was ok, 1 otherwise
int try_xma_riff(const uint8_t *infile, long file_size, subfile_callback_t *cb, void *cbv)
{
    uint32_t riff_size = 0;
    uint32_t data_size = 0;
    long start_offset = -1;

    long block_size = -1;
    int channel_count = 0;
    int stream_count = 0;
    long sample_count = -1;
    long loop_start = -1;
    long loop_end = -1;
    int sample_rate = 0;

    int FormatChunkFound = 0;
    int DataChunkFound = 0;
    int XMA2ChunkFound = 0;

    int * stream_channels = NULL;
    int i;

    /* check header */
    if ((uint32_t)read_32_be(&infile[0])!=0x52494646) /* "RIFF" */
        goto fail;
    /* check for WAVE form */
    if ((uint32_t)read_32_be(&infile[8])!=0x57415645) /* "WAVE" */
        goto fail;

    riff_size = read_32_le(&infile[4]);

    /* check for tructated RIFF */
    if (file_size < riff_size+8) goto fail;

    /* read through chunks to verify format and find metadata */
    {
        long current_chunk = 0xc; /* start with first chunk */

        while (current_chunk < file_size && current_chunk < riff_size+8) {
            uint32_t chunk_type = read_32_be(&infile[current_chunk]);
            long chunk_size = read_32_le(&infile[current_chunk+4]);

            if (current_chunk+8+chunk_size > file_size) goto fail;

            switch(chunk_type) {
                case 0x666d7420:    /* "fmt " */
                    /* only one per file */
                    if (FormatChunkFound) goto fail;
                    FormatChunkFound = 1;

                    sample_rate = read_32_le(&infile[current_chunk+0x0c]);
                    channel_count = read_16_le(&infile[current_chunk+0x0a]);

                    /* coding */
                    if (0x166 != read_16_le(&infile[current_chunk+0x8]))
                        goto fail;

                    /* XMA2 extra stuff */
                    if (0x22 != read_16_le(&infile[current_chunk+0x18]))
                        goto fail;

                    stream_count = read_16_le(&infile[current_chunk+0x1a]);
                    sample_count = read_32_le(&infile[current_chunk+0x20]);
                    block_size = read_32_le(&infile[current_chunk+0x24]);
#if 0
                    play_start = read_32_le(&infile[current_chunk+0x28]);
                    play_length = read_32_le(&infile[current_chunk+0x2c]);
#endif
                    loop_start = read_32_le(&infile[current_chunk+0x30]);
                    loop_end = loop_start+read_32_le(&infile[current_chunk+0x34]);
                    /* don't count first frame */
                    sample_count -= samples_per_frame;
                    break;
                case 0x64617461:    /* data */
                    /* at most one per file */
                    if (DataChunkFound) goto fail;
                    DataChunkFound = 1;

                    start_offset = current_chunk + 8;
                    data_size = chunk_size;
                    break;
#if 0
                case 0x66616374:    /* fact */
                    if (chunk_size != 4) break;
                    sample_count = read_32_le(&infile[current_chunk+8]);
                    break;
#endif
                case 0x584D4132:    /* XMA2 */
                    if (XMA2ChunkFound) goto fail;
                    XMA2ChunkFound = 1;

                    sample_rate = read_32_be(&infile[current_chunk+0x14]);

                    stream_count = infile[current_chunk+0x9];
                    sample_count = read_32_be(&infile[current_chunk+0x24]);
                    block_size = read_32_be(&infile[current_chunk+0x20]);
                    loop_start = read_32_be(&infile[current_chunk+0xC]);
                    loop_end = read_32_be(&infile[current_chunk+0x10]);

                    stream_channels = calloc(stream_count, sizeof(int));
                    for (i = 0; i < stream_count; i++)
                    {
                        stream_channels[i] = infile[current_chunk+0x30+i*4];
                    }

                    break;
                    
                default:
                    /* ignorance is bliss */
                    break;
            }

            current_chunk += 8+chunk_size;
        }
    }

    if (!(FormatChunkFound || XMA2ChunkFound) || !DataChunkFound) goto fail;

    return cb(&infile[start_offset], data_size, stream_count, stream_channels, sample_count, sample_rate, block_size, loop_start, loop_end, "RIFF_", cbv);

fail:
    return 1;
}
