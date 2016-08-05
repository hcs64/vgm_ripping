#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "error_stuff.h"
#include "util.h"

struct dsp_header {
    uint32_t sample_count;
    uint32_t nibble_count;
    uint32_t sample_rate;
    uint16_t loop_flag;
    uint16_t format;
    uint32_t loop_start_offset;
    uint32_t loop_end_offset;
};

uint32_t dsp_nibbles_to_samples(uint32_t nibbles) {
    uint32_t whole_frames = nibbles/16;
    unsigned int remainder = nibbles%16;

    if (remainder>0) return whole_frames*14+remainder-2;
    else return whole_frames*14;
}

void read_dsp_header(struct dsp_header * h, FILE * in, long offset) {
    h->sample_count = get_32_be_seek(offset, in);
    h->nibble_count = get_32_be(in);
    h->sample_rate  = get_32_be(in);
    h->loop_flag    = get_16_be(in);
    h->format       = get_16_be(in);
    h->loop_start_offset = get_32_be(in);
    h->loop_end_offset = get_32_be(in);
}

int main(int argc, char **argv)
{
    const char *infile_name = NULL;
    const char *outfile_name = NULL;
    if (argc != 3) {
        fprintf(stderr, "build GENH from Taiko no Tatsukin Atsumete Tomodachi Daisakusen .nus3bank\n");
        fprintf(stderr, "usage: ttatd SONG_YUGEN.nus3bank SONG_YUGEN.genh\n");
        return -1;
    }

    infile_name = argv[1];
    outfile_name = argv[2];

    FILE *infile = fopen(infile_name, "rb");
    CHECK_ERRNO(NULL == infile, "input file open failed");

    const long nus3_offset = 0;
    EXPECT_32_BE(nus3_offset, infile, 0x4E555333, "NUS3");
    const long nus3_size = get_32_le_seek(nus3_offset + 4, infile) + 8;
    const long nus3_end = nus3_offset + nus3_size;

    EXPECT_32_BE(nus3_offset + 8, infile, 0x42414E4B, "BANK");

    long chunk_offset = nus3_offset + 12;

    long PACK_offset = -1;
    uint32_t PACK_size = 0;

    /* scan to find PACK */
    while (chunk_offset < nus3_end)
    {
        uint32_t chunk_type = get_32_be_seek(chunk_offset, infile);
        long     chunk_size = get_32_le(infile);

        //printf("offset: %"PRIx32" type: %"PRIx32" size: %"PRIx32"\n", (uint32_t)chunk_offset, chunk_type, (uint32_t)chunk_size);

        switch (chunk_type)
        {
            case UINT32_C(0x544F4320):  /* TOC  */
            case UINT32_C(0x50524F50):  /* PROP */
            case UINT32_C(0x42494E46):  /* BINF */
            case UINT32_C(0x47525020):  /* GRP  */
            case UINT32_C(0x44544F4E):  /* DTON */
            case UINT32_C(0x544F4E45):  /* TONE */
            case UINT32_C(0x4A554E4B):  /* JUNK */
                break;

            case UINT32_C(0x5041434B):  /* PACK */
                CHECK_ERROR(PACK_offset != -1, "expected only one PACK");
                PACK_offset = chunk_offset + 8;
                PACK_size = chunk_size;
                break;

            default:
                fprintf(stderr, "unexpected chunk %"PRIx32" at 0x%lx\n", chunk_type, chunk_offset);
                CHECK_ERROR(1, "who knows?");
                break;
        }
        chunk_offset += 8 + chunk_size;
        CHECK_ERROR(chunk_offset > nus3_end, "truncated chunk");
    }

    CHECK_ERROR(PACK_offset == -1, "missing PACK");

    const long IDSP_offset = PACK_offset;
    EXPECT_32_BE(IDSP_offset, infile, 0x49445350, "IDSP");

    EXPECT_32_BE(IDSP_offset + 4, infile, 0, "0 at IDSP+4");
    const uint32_t channels    = get_32_be_seek(IDSP_offset + 0x08, infile);
    CHECK_ERROR(channels != 1 && channels != 2,
        "only mono and stereo supported in GENH");

    const uint32_t sample_rate = get_32_be_seek(IDSP_offset + 0x0c, infile);
    const uint32_t sample_count= get_32_be_seek(IDSP_offset + 0x10, infile);
    EXPECT_32_BE(IDSP_offset + 0x14, infile, 0, "0 at IDSP+0x14");
    EXPECT_32_BE(IDSP_offset + 0x18, infile, 0, "0 at IDSP+0x18");
    EXPECT_32_BE(IDSP_offset + 0x1c, infile, 0, "0 at IDSP+0x1c");
    const uint32_t ch_head_offset = get_32_be_seek(IDSP_offset + 0x20, infile);
    const uint32_t ch_head_size   = get_32_be_seek(IDSP_offset + 0x24, infile);
    const uint32_t ch_body_offset = get_32_be_seek(IDSP_offset + 0x28, infile);
    const uint32_t ch_body_size   = get_32_be_seek(IDSP_offset + 0x2c, infile);

    CHECK_ERROR(ch_head_size != 0x60, "expected 0x60 header per channel");

    int loop_flag = 0;
    int loop_start = 0;
    int loop_end = 0;

    /* read header for checking and loop info */
    for (unsigned int c = 0; c < channels; c++) {
        struct dsp_header h;
        read_dsp_header(&h, infile, IDSP_offset + ch_head_offset + ch_head_size * c);

        CHECK_ERROR(h.sample_count != sample_count, "sample count mismatch");
        CHECK_ERROR(dsp_nibbles_to_samples(h.nibble_count) != sample_count, "nibble count mismatch");
        CHECK_ERROR(h.sample_rate  != sample_rate,  "sample rate mismatch");
        CHECK_ERROR(h.format != 0, "not DSP format?");

        CHECK_ERROR(c > 0 && ((h.loop_flag && !loop_flag) || (!h.loop_flag && loop_flag)), "loop flag mismatch");

        if (h.loop_flag) {
            loop_flag = 1;
            uint32_t new_loop_start = dsp_nibbles_to_samples(h.loop_start_offset);
            uint32_t new_loop_end   = dsp_nibbles_to_samples(h.loop_end_offset);
            if (c > 0) {
                CHECK_ERROR(loop_start != new_loop_start || loop_end != new_loop_end, "loop point mismatch");
            }

            loop_start = new_loop_start;
            loop_end = new_loop_end;
        }
    }


    long IDSP_size = ch_head_offset + (ch_head_size + ch_body_size) * channels;
    CHECK_ERROR(IDSP_size != PACK_size, "IDSP size does not match PACK size");

    /* build GENH */
    unsigned char genh_head[0x38];
    memset(genh_head, 0, sizeof(genh_head));

    /* header magic */
    memcpy(&genh_head[0x00], "GENH", 4);

    /* channel count */
    write_32_le(channels, &genh_head[0x04]);

    /* interleave */
    if (channels == 1)
    {
      write_32_le(0, &genh_head[0x08]);
    }
    else if (channels == 2)
    {
      // no interleave, but GENH does split data like this
      write_32_le(ch_body_size, &genh_head[0x08]);
    }

    /* sample rate */
    write_32_le(sample_rate, &genh_head[0x0c]);

    /* loop start */
    if (loop_flag) {
        write_32_le(loop_start, &genh_head[0x10]);
    } else {
        write_32_le(~UINT32_C(0), &genh_head[0x10]);
    }

    /* loop end */
    if (loop_flag) {
        write_32_le(loop_end, &genh_head[0x14]);
    } else {
        write_32_le(sample_count, &genh_head[0x14]);
    }

    /* coding type, coding_NGC_DSP */
    write_32_le(12, &genh_head[0x18]);

    /* start_offset */
    write_32_le(0x38 + IDSP_offset + ch_body_offset, &genh_head[0x1c]);

    /* header_size */
    write_32_le(0x38, &genh_head[0x20]);

    /* dsp coefs */
    write_32_le(0x38 + IDSP_offset + ch_head_offset + 0x1c, &genh_head[0x24]);
    if (channels == 2) {
        write_32_le(0x38 + IDSP_offset + ch_head_offset + ch_head_size + 0x1c, &genh_head[0x28]);
    }

    /* dsp interleave type */
    if (channels == 1) {
        /* no interleave */
          write_32_le(2, &genh_head[0x2c]);
    }

    /* normal coefs */
    //write_32_le(0, &genh_head[0x30]);

    FILE *outfile = fopen(outfile_name, "wb");
    CHECK_ERRNO(NULL == outfile, "output file open failed");

    put_bytes(outfile, genh_head, sizeof(genh_head));
    dump(infile, outfile, nus3_offset, nus3_size);

    CHECK_ERRNO(EOF == fclose(outfile), "fclose of output file");
}
