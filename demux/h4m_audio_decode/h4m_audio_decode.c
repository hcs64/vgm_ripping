#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

/* .h4m (HVQM4 1.3/1.5) audio decoder 0.3 by hcs */

//#define VERBOSE_PRINT

/* big endian */

static uint8_t get8(FILE *infile)
{
    uint8_t buf[1];
    if (1 != fread(buf, 1, 1, infile))
    {
        fprintf(stderr, "read error at 0x%lx\n", (unsigned long)ftell(infile));
        exit(EXIT_FAILURE);
    }

    return buf[0];
}

static uint16_t read16(uint8_t * buf)
{
    uint32_t v = 0;
    for (int i = 0; i < 2; i++)
    {
        v <<= 8;
        v |= buf[i];
    }
    return v;
}

static uint16_t get16(FILE * infile)
{
    uint8_t buf[2];
    if (2 != fread(buf, 1, 2, infile))
    {
        fprintf(stderr, "read error at 0x%lx\n", (unsigned long)ftell(infile));
        exit(EXIT_FAILURE);
    }
    return read16(buf);
}

static uint32_t read32(uint8_t * buf)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
    {
        v <<= 8;
        v |= buf[i];
    }
    return v;
}

static uint32_t get32(FILE * infile)
{
    uint8_t buf[4];
    if (4 != fread(buf, 1, 4, infile))
    {
        fprintf(stderr, "read error at 0x%lx\n", (unsigned long)ftell(infile));
        exit(EXIT_FAILURE);
    }
    return read32(buf);
}

static void expect32(uint32_t expected, FILE * infile)
{
    uint32_t v = get32(infile);
    if (v != expected)
    {
        fprintf(stderr, "expected 0x%08"PRIx32" at 0x%lx, got 0x%08"PRIx32"\n",
            expected, ftell(infile)-4, v);
        exit(EXIT_FAILURE);
    }
}

static void expect32_imm(uint32_t expected, uint32_t actual, unsigned long offset)
{
    if (expected != actual)
    {
        fprintf(stderr, "expected 0x%08lx to be 0x%08"PRIx32", got 0x%08"PRIx32"\n",
            offset, expected, actual);
        exit(EXIT_FAILURE);
    }
}

static void expect32_text(uint32_t expected, uint32_t actual, char *name)
{
    if (expected != actual)
    {
        fprintf(stderr, "expected %s to be 0x%08"PRIx32", got 0x%08"PRIx32"\n",
            name, expected, actual);
        exit(EXIT_FAILURE);
    }
}

static void seek_past(uint32_t offset, FILE * infile)
{
    if (-1 == fseek(infile, offset, SEEK_CUR))
    {
        fprintf(stderr, "seek by 0x%x failed\n", offset);
        exit(EXIT_FAILURE);
    }
}

/* audio decode */

struct audio_state
{
    struct
    {
        int16_t hist;
        int8_t idx;
    } *ch;
};

const int32_t IMA_Steps[89] =
{
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767

};

const int8_t IMA_IndexTable[16] = 

{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8 
};

static int16_t clamp16(int32_t v)
{
    if (v > INT16_MAX) return INT16_MAX;
    else if (v < INT16_MIN) return INT16_MIN;
    return v;
}

static void decode_audio(struct audio_state *state, int first_aud, uint32_t sample_count, FILE *infile, FILE *outfile, int channels)
{
    int16_t * samples = samples = malloc(sample_count*sizeof(int16_t)*channels);
    uint32_t t = 0, i = 0;

    if (first_aud)
    {
        for (int c = channels - 1; c >= 0; c--)
        {
            state->ch[c].hist = get8(infile);
            state->ch[c].hist <<= 8;
            uint8_t b = get8(infile);
            state->ch[c].hist |= (b & 0x80);
            state->ch[c].idx = b & 0x7f;

            if (state->ch[c].idx > 88)
            {
                fprintf(stderr, "invalid step index (%d) at 0x%lx\n", state->ch[c].idx, ftell(infile));
                exit(EXIT_FAILURE);
            }
        }

        for (int c = 0; c < channels; c++)
        {
            samples[t++] = state->ch[c].hist;
        }
        i ++;
    }

    uint8_t b;
    int bitsleft = 0;
    for (; i < sample_count; i++)
    {
        for (int c = channels-1; c >= 0; c--)
        {
            if (bitsleft == 0)
            {
                b = get8(infile);
                bitsleft = 8;
            }

            int32_t ima_step = IMA_Steps[state->ch[c].idx];
            int32_t ima_delta = ima_step >> 3;
            if (b & 0x10) ima_delta += ima_step >> 2;
            if (b & 0x20) ima_delta += ima_step >> 1;
            if (b & 0x40) ima_delta += ima_step;

            if (b & 0x80) state->ch[c].hist = clamp16(state->ch[c].hist - ima_delta);
            else state->ch[c].hist = clamp16(state->ch[c].hist + ima_delta);
            state->ch[c].idx += IMA_IndexTable[(b&0xf0)>>4];
            if (state->ch[c].idx > 88) state->ch[c].idx = 88;
            if (state->ch[c].idx < 0) state->ch[c].idx = 0;

            b <<= 4;
            bitsleft -= 4;
        }

        for (int c = 0; c < channels; c++)
        {
            samples[t++] = state->ch[c].hist;
        }
    }

    if (sample_count != fwrite(samples, sizeof(int16_t)*channels, sample_count, outfile))
    {
        fprintf(stderr, "error writing output\n");
        exit(EXIT_FAILURE);
    }

    free(samples);
}

/* stream structure */

const char HVQM4_13_magic[16] = "HVQM4 1.3";
const char HVQM4_15_magic[16] = "HVQM4 1.5";

struct HVQM4_header
{
    enum
    {
        HVQM4_13,
        HVQM4_15,
    } version;

    uint32_t header_size;   /* 0x10-0x13 */
    uint32_t body_size;     /* 0x14-0x17 */
    uint32_t blocks;        /* 0x18-0x1B */
    uint32_t audio_frames;  /* 0x1C-0x1F */
    uint32_t video_frames;  /* 0x20-0x23 */
    uint32_t unk24;         /* 0x24-0x27 (0x8257,0x8256) */
    uint32_t duration;      /* 0x28-0x2B */
    uint32_t unk2C;         /* 0x2C-0x2F (0) */
    uint32_t audio_frame_sz;/* 0x30-0x33 */
    uint16_t hres;          /* 0x34-0x35 */
    uint16_t vres;          /* 0x36-0x37 */
    uint16_t unk38;         /* 0x38-0x3B (0x0202) */
    uint8_t  unk3A;         /* 0x3A (0 or 0x12) */
    uint8_t  unk3B;         /* 0x3B (0) */
    uint8_t  audio_channels;/* 0x3C */
    uint8_t  audio_bitdepth;/* 0x3D */
    uint16_t pad;           /* 0x3E-0x3F (0) */
    uint32_t audio_srate;   /* 0x40-0x43 */
};

static void load_header(struct HVQM4_header *header, uint8_t *raw_header)
{
    /* check MAGIC */
    if (!memcmp(HVQM4_13_magic, &raw_header[0], 16))
    {
        header->version = HVQM4_13;
    }
    else if (!memcmp(HVQM4_15_magic, &raw_header[0], 16))
    {
        header->version = HVQM4_15;
    }
    else
    {
        fprintf(stderr, "does not appear to be a HVQM4 file\n");
        exit(EXIT_FAILURE);
    }

    header->header_size = read32(&raw_header[0x10]);
    header->body_size = read32(&raw_header[0x14]);
    header->blocks = read32(&raw_header[0x18]);
    header->audio_frames = read32(&raw_header[0x1C]);
    header->video_frames = read32(&raw_header[0x20]);
    header->unk24 = read32(&raw_header[0x24]);
    header->duration = read32(&raw_header[0x28]);
    header->unk2C = read32(&raw_header[0x2C]);
    header->audio_frame_sz = read32(&raw_header[0x30]);
    header->hres = read16(&raw_header[0x34]);
    header->vres = read16(&raw_header[0x36]);
    header->unk38 = read16(&raw_header[0x38]);
    header->unk3A = raw_header[0x3A];
    header->unk3B = raw_header[0x3B];
    header->audio_channels = raw_header[0x3C];
    header->audio_bitdepth = raw_header[0x3D];
    header->pad = read16(&raw_header[0x3E]);
    header->audio_srate = read32(&raw_header[0x40]);

    expect32_text(0x44, header->header_size, "header size");
    /* no check for body size yet */
    if (header->blocks == 0)
    {
        fprintf(stderr, "zero blocks\n");
        exit(EXIT_FAILURE);
    }
    if (header->audio_frames != 0)
    {
        if (header->audio_srate == 0 || header->audio_frame_sz == 0 || header->audio_channels == 0)
        {
            fprintf(stderr, "expected nonzero audio srate and frame size\n");
            exit(EXIT_FAILURE);
        }
    }
    /* no check for video frame count */
    /*
    if (header->unk24 != 0x8257 && header->unk24 != 0x8256)
    {
        expect32_imm(0x8257, header->unk24, 0x24);
    }
    */
    expect32_imm(0, header->unk2C, 0x2C);
    //expect32_text(0x650, header->audio_frame_sz, "audio frame size");
    /* no check for hres, vres */
    expect32_imm(0x0202, header->unk38, 0x38);
    if (header->unk3A != 0 && header->unk3A != 0x12)
    {
        expect32_imm(0, header->unk3A, 0x3A);
    }
    expect32_imm(0, header->unk3B, 0x3B);
    //expect32_imm(0x02100000, header->unk3C, 0x3C);
    /* no check for srate, can be 0 */
}

void display_header(struct HVQM4_header *header)
{
    switch (header->version)
    {
        case HVQM4_13:
            printf("HVQM4 1.3\n");
            break;
        case HVQM4_15:
            printf("HVQM4 1.5\n");
            break;
    }
    printf("Header size: 0x%"PRIx32"\n", header->header_size);
    printf("Body size:   0x%"PRIx32"\n", header->body_size);
    printf("Duration:    %"PRIu32" (?)\n", header->duration);
    printf("Resolution:  %"PRIu32" x %"PRIu32"\n", header->hres, header->vres);
    printf("unk24:       0x%"PRIu32"\n", header->unk24);
    printf("%d Blocks\n", header->blocks);
    printf("%d Video frames\n", header->video_frames);
    if (header->audio_frames)
    {
        printf("%d Audio frames\n", header->audio_frames);
        printf("Sample rate: %"PRIu32" Hz\n", header->audio_srate);
        printf("Audio frame size: 0x%"PRIx32"\n", header->audio_frame_sz);
        printf("Audio channels: %u\n", header->audio_channels);
    } else {
        printf("No audio!\n");
    }
    printf("\n");
}

static void put_32bitLE(uint8_t * buf, uint32_t v)
{
    for (unsigned int i = 0; i < 4; i++)
    {
        buf[i] = v & 0xFF;
        v >>= 8;
    }
}

static void put_16bitLE(uint8_t * buf, uint16_t v)
{
    for (unsigned int i = 0; i < 2; i++)
    {
        buf[i] = v & 0xFF;
        v >>= 8;
    }
}

/* make a header for 16-bit PCM .wav */
/* buffer must be 0x2c bytes */
static void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels) {
    size_t bytecount;

    bytecount = sample_count*channels*2;

    /* RIFF header */
    memcpy(buf+0, "RIFF", 4);
    /* size of RIFF */
    put_32bitLE(buf+4, (int32_t)(bytecount+0x2c-8));

    /* WAVE header */
    memcpy(buf+8, "WAVE", 4);

    /* WAVE fmt chunk */
    memcpy(buf+0xc, "fmt ", 4);
    /* size of WAVE fmt chunk */
    put_32bitLE(buf+0x10, 0x10);

    /* compression code 1=PCM */
    put_16bitLE(buf+0x14, 1);

    /* channel count */
    put_16bitLE(buf+0x16, channels);

    /* sample rate */
    put_32bitLE(buf+0x18, sample_rate);

    /* bytes per second */
    put_32bitLE(buf+0x1c, sample_rate*channels*2);

    /* block align */
    put_16bitLE(buf+0x20, (int16_t)(channels*2));

    /* significant bits per sample */
    put_16bitLE(buf+0x22, 2*8);

    /* PCM has no extra format bytes, so we don't even need to specify a count */

    /* WAVE data chunk */
    memcpy(buf+0x24, "data", 4);
    /* size of WAVE data chunk */
    put_32bitLE(buf+0x28, (int32_t)bytecount);
}

int main(int argc, char **argv)
{
    printf("h4m 'HVQM4 1.3/1.5' audio decoder 0.3 by hcs\n\n");
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s file.h4m output.wav\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* open input */
    FILE *infile = fopen(argv[1], "rb");
    if (!infile)
    {
        fprintf(stderr, "failed opening %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    /* read in header */
    uint8_t raw_header[0x44];
    if (0x44 != fread(&raw_header, 1, 0x44, infile))
    {
        fprintf(stderr, "failed reading header");
        exit(EXIT_FAILURE);
    }

    /* load up and check header */
    struct HVQM4_header header;
    load_header(&header, raw_header);
    display_header(&header);

    if (header.audio_frames == 0)
    {
        fprintf(stderr, "this video contains no audio!\n");
        exit(EXIT_FAILURE);
    }

    /* open output */
    FILE *outfile = fopen(argv[2], "wb");
    if (!outfile)
    {
        fprintf(stderr, "error opening %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    /* fill in the space that we'll put the header in later */
    uint8_t riff_header[0x2c];
    if (0x2c != fwrite(riff_header, 1, 0x2c, outfile))
    {
        fprintf(stderr, "error writing riff header\n");
        exit(EXIT_FAILURE);
    }
    
    /* parse blocks */
    uint32_t block_count = 0;
    uint32_t total_aud_frames = 0;
    uint32_t total_vid_frames = 0;
    uint32_t last_block_start = ftell(infile);
    uint32_t total_sample_count = 0;
    while (block_count < header.blocks)
    {
        const long block_start = ftell(infile);
        expect32(ftell(infile) - last_block_start, infile);
        last_block_start = block_start;
        const uint32_t expected_block_size = get32(infile);
        const uint32_t expected_aud_frame_count = get32(infile);
        const uint32_t expected_vid_frame_count = get32(infile);
        expect32(0x01000000, infile);   /* EOS marker? */
        const long data_start = ftell(infile);

        block_count ++;
#ifdef VERBOSE_PRINT
        printf("block %d starts at 0x%lx, length 0x%"PRIx32"\n", (int)block_count, block_start, expected_block_size);
#endif

        /* parse frames */
        struct audio_state audio_state;
        int first_vid=1, first_aud=1;
        uint32_t vid_frame_count = 0, aud_frame_count = 0;
        int block_sample_count =0;

        audio_state.ch = calloc(header.audio_channels, sizeof(*audio_state.ch));
        while (aud_frame_count < expected_aud_frame_count ||
               vid_frame_count < expected_vid_frame_count)
        {
            const uint16_t frame_id1 = get16(infile);
            const uint16_t frame_id2 = get16(infile);
            const uint32_t frame_size = get32(infile);

#ifdef VERBOSE_PRINT
            printf("frame id 0x%"PRIx16",0x%"PRIx16" ",frame_id1,frame_id2);
            printf("size 0x%"PRIx32"\n", frame_size);
#endif

            if (frame_id1 == 1 && (
                        (header.version == HVQM4_13 && frame_id2 == 0x10) ||
                        (header.version == HVQM4_13 && frame_id2 == 0x30) ||
                        (first_vid && frame_id2 == 0x10) ||
                        (!first_vid && frame_id2 == 0x20)))
            {
                /* video */
                first_vid = 0;
                vid_frame_count ++;
                total_vid_frames ++;
#ifdef VERBOSE_PRINT
                printf("video frame %d/%d (%d)\n", (int)vid_frame_count, (int)expected_vid_frame_count, (int)total_vid_frames);
#endif
                seek_past(frame_size, infile);
            }
            else if (frame_id1 == 0 &&
                ((first_aud && ( frame_id2 == 3 || frame_id2 == 1)) ||
                (!first_aud && frame_id2 == 2)))

            {
                /* audio */
                const long audio_started = ftell(infile);
                const uint32_t samples = get32(infile);
                decode_audio(&audio_state, first_aud, samples, infile, outfile, header.audio_channels);
                block_sample_count += samples;
                aud_frame_count ++;
                total_aud_frames ++;
#ifdef VERBOSE_PRINT
                printf("0x%lx: audio frame %d/%d (%d) (%d samples)\n", (unsigned long)audio_started, (int)aud_frame_count, (int)expected_aud_frame_count, (int)total_aud_frames, samples);
#endif
                first_aud = 0;
                long bytes_done_unto = ftell(infile) - audio_started;
                if (bytes_done_unto > frame_size)
                {
                    fprintf(stderr, "processed 0x%lx bytes, should have done 0x%"PRIx32"\n",
                        bytes_done_unto, frame_size);
                    exit(EXIT_FAILURE);
                }
                else if (bytes_done_unto < frame_size)
                {
                    while (bytes_done_unto < frame_size)
                    {
                        get8(infile);
                        bytes_done_unto ++;
                    }
                }
            }
            else
            {
                fprintf(stderr, "unexpected frame id at %08lx\n", (unsigned long)ftell(infile));
                exit(EXIT_FAILURE);
                printf("unexpected frame id %d %d at %08lx\n", frame_id1, frame_id2, (unsigned long)(ftell(infile)-8));
                seek_past(frame_size, infile);
            }
        }

        if (expected_aud_frame_count != aud_frame_count ||
            expected_vid_frame_count != vid_frame_count)
        {
            fprintf(stderr, "frame count mismatch\n");
            exit(EXIT_FAILURE);
        }

#ifdef VERBOSE_PRINT
        printf("block %d ended at 0x%lx (%d samples)\n", (int)block_count, ftell(infile), block_sample_count);
#endif
        if (ftell(infile) != (data_start+expected_block_size))
        {
            fprintf(stderr, "block size mismatch\n");
            exit(EXIT_FAILURE);
        }
        total_sample_count += block_sample_count;
    }

    if (total_aud_frames != header.audio_frames ||
        total_vid_frames != header.video_frames)
    {
        fprintf(stderr, "total frame count mismatch\n");
        exit(EXIT_FAILURE);
    }

    printf("%"PRIu32" samples\n", total_sample_count);

    // generate header
    make_wav_header(riff_header, total_sample_count, header.audio_srate, header.audio_channels);
    fseek(outfile, 0, SEEK_SET);
    if (0x2c != fwrite(riff_header, 1, 0x2c, outfile))
    {
        fprintf(stderr, "error rewriting riff header\n");
        exit(EXIT_FAILURE);
    }
    if (EOF == fclose(outfile))
    {
        fprintf(stderr, "error finishing output\n");
        exit(EXIT_FAILURE);
    }

    printf("Done!\n");
}
