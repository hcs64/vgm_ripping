#include <inttypes.h>
#include "gcdsp_decoder.h"
#include "util.h"

void decode_gcdsp(int16_t * outbuf, off_t start_offset, int channelspacing, STREAMFILE * streamfile, int32_t first_sample, int32_t samples_to_do, int32_t * phist1, int32_t * phist2, int16_t coefs[]) {
    int i=first_sample;
    int32_t sample_count;

    int framesin = first_sample/14;

    int8_t header = read_8bit(framesin*8+start_offset,streamfile);
    int32_t scale = 1 << (header & 0xf);
    int coef_index = (header >> 4) & 0xf;
    int32_t hist1 = *phist1;
    int32_t hist2 = *phist2;
    int coef1 = coefs[coef_index*2];
    int coef2 = coefs[coef_index*2+1];

    first_sample = first_sample%14;

    for (i=first_sample,sample_count=0; i<first_sample+samples_to_do; i++,sample_count+=channelspacing) {
        int sample_byte = read_8bit(framesin*8+start_offset+1+i/2,streamfile);

        outbuf[sample_count] = clamp16((
                 (((i&1?
                    get_low_nibble_signed(sample_byte):
                    get_high_nibble_signed(sample_byte)
                   ) * scale)<<11) + 1024 +
                 (coef1 * hist1 + coef2 * hist2))>>11
                );

        hist2 = hist1;
        hist1 = outbuf[sample_count];
    }

    *phist1 = hist1;
    *phist2 = hist2;
}
