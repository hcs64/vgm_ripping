#ifndef _XMA_REBUILD_H
#define _XMA_REBUILD_H

#include <stdint.h>

enum {
    xma_header_size = 0x3c,
    packet_size_bytes = 0x800,
    packet_header_size_bytes = 4,
    frame_header_size_bits = 15,
    frame_sync_size_bits = 15,
    frame_skip_size_bits = 10,
    frame_trailer_size_bits = 1,
    samples_per_frame = 512,
    default_block_size = 0x8000
};

uint8_t *make_xma_header(uint32_t srate, uint32_t size, int channels);

// return 0 on success, 1 if a parse error was encountered
int build_XMA_from_XMA2(const uint8_t *indata, long data_size, FILE *outfile, long block_size, int channels, long *samples_p);

#endif // _XMA_REBUILD_H
