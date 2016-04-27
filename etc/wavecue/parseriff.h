#ifndef _PARSERIFF_H_INCLUDED
#define _PARSERIFF_H_INCLUDED

#include <stdint.h>
#include <inttypes.h>
#include "util.h"
#include "error_stuff.h"

static const uint32_t RIFF_MAGIC = UINT32_C(0x52494646);
static const uint32_t WAVE_MAGIC = UINT32_C(0x57415645);

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

void check_riff_wave(FILE * infile, long * first_chunk_p, long * wave_end_p);

int find_chunks(FILE *infile, const long first_chunk, const long file_end, struct chunk_info * chunk_info, const int chunk_info_count, const int ignore_unknown);

#endif
