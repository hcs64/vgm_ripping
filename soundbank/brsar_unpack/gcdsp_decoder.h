#include <inttypes.h>
#include "streamfile.h"

#ifndef _GCDSP_DECODER_H
#define _GCDSP_DECODER_H

void decode_gcdsp(int16_t * outbuf, off_t start_offset, int channelspacing, STREAMFILE * streamfile, int32_t first_sample, int32_t samples_to_do, int32_t * phist1, int32_t * phist2, int16_t coefs[]);

#endif
