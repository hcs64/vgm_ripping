#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

/* CSL 0.0 */
/* extract and decode DSP from CSL files from Baten Kaitos */
/* This is likely a whole sequence format, but here it looks like we have
   files which each contain a minimal sequence and a single program with a
   single sample. Lots of checks to hopefully catch it when something else
   pops up. */

/* big endian */

uint16_t read16(uint8_t * buf)
{
    uint32_t v = 0;
    for (unsigned int i = 0; i < 2; i++)
    {
        v <<= 8;
        v |= buf[i];
    }
    return v;
}

uint16_t get16(FILE * infile)
{
    uint8_t buf[2];
    if (2 != fread(buf, 1, 2, infile))
    {
        fprintf(stderr, "read error at 0x%lx\n", (unsigned long)ftell(infile));
        exit(EXIT_FAILURE);
    }
    return read16(buf);
}

void expect16(uint16_t expected, FILE * infile)
{
    uint32_t v = get16(infile);
    if (v != expected)
    {
        fprintf(stderr, "expected 0x%08"PRIx16" at 0x%lx, got 0x%08"PRIx16"\n",
            expected, ftell(infile)-2, v);
        exit(EXIT_FAILURE);
    }
}

uint32_t read32(uint8_t * buf)
{
    uint32_t v = 0;
    for (unsigned int i = 0; i < 4; i++)
    {
        v <<= 8;
        v |= buf[i];
    }
    return v;
}

uint32_t get32(FILE * infile)
{
    uint8_t buf[4];
    if (4 != fread(buf, 1, 4, infile))
    {
        fprintf(stderr, "read error at 0x%lx\n", (unsigned long)ftell(infile));
        exit(EXIT_FAILURE);
    }
    return read32(buf);
}

void expect32(uint32_t expected, FILE * infile)
{
    uint32_t v = get32(infile);
    if (v != expected)
    {
        fprintf(stderr, "expected 0x%08"PRIx32" at 0x%lx, got 0x%08"PRIx32"\n",
            expected, ftell(infile)-4, v);
        exit(EXIT_FAILURE);
    }
}

void seek(uint32_t offset, FILE * infile)
{
    if (-1 == fseek(infile, offset, SEEK_SET))
    {
        fprintf(stderr, "seek to 0x%x failed\n", offset);
        exit(EXIT_FAILURE);
    }
}

void decode_dsp(char * basename, uint32_t id, uint32_t coef_offset,
    uint32_t data_offset, uint32_t sample_count, FILE * infile);
void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels);

const uint32_t CSL_magic  = UINT32_C(0x43534C20);
const uint32_t CSF_magic  = UINT32_C(0x43534620);
const uint32_t BOOK_magic = UINT32_C(0x424F4F4B);
const uint32_t SONG_magic = UINT32_C(0x534F4E47);
const uint32_t PGHD_magic = UINT32_C(0x50474844);
const uint32_t PROG_magic = UINT32_C(0x50524F47);
const uint32_t TIM_magic  = UINT32_C(0x54494D20);

int main(int argc, char **argv)
{
    printf("CSL decoder 0.0\n");
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s blah.csl\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *infile = fopen(argv[1],"rb");
    if (!infile)
    {
        fprintf(stderr, "failed opening %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    expect32(CSL_magic, infile);

    const uint32_t csf_count = get32(infile);
    const uint32_t csf_table = ftell(infile);
    printf("%d CSFs\n", csf_count);

    /* each CSF */
    uint16_t song_id;
    uint32_t song_id2;
    for (int csf_no = 0; csf_no < csf_count; csf_no++)
    {
        seek(csf_table + csf_no * 4, infile);
        const uint32_t csf_offset = get32(infile);
        seek(csf_offset, infile);
        expect32(CSF_magic, infile);

        const uint32_t csf_size = get32(infile);
        const uint32_t csf_header_size = get32(infile);
        const uint32_t csf_body_size = get32(infile);

        printf("%d 0x%"PRIx32" (0x%"PRIx32" 0x%"PRIx32" 0x%"PRIx32")\n",
                csf_no, csf_offset, csf_header_size, csf_body_size, csf_size);

        if (csf_header_size != 0xc0)
        {
            fprintf(stderr, "expected header size 0xc0, got %"PRIx32"\n",
                csf_header_size);
            exit(EXIT_FAILURE);
        }

        /* BOOK chunk (set of sequences) */
        {
            expect32(BOOK_magic, infile);   /* 00: 'BOOK' */
            expect32(0x40, infile);         /* 04: size */
            expect32(0x01, infile);         /* 08: song count? */
            expect32(0x00, infile);         /* 0C: padding? */

            /* SONG chunk (sequence) */
            {
                expect32(SONG_magic, infile);   /* 00: 'SONG' */
                expect32(0x30, infile);         /* 04: size */

                /* 08: id */
                const uint16_t new_id = get16(infile);
                if (csf_no > 0)
                {
                    if (new_id != song_id+1)
                    {
                        fprintf(stderr, "expected new song id 0x%"PRIx16", got 0x%"PRIx16"\n",
                            song_id+1, new_id);
                        exit(EXIT_FAILURE);
                    }
                }
                song_id = new_id;

                /* 0A */
                const uint32_t thingy = get32(infile);
                switch (thingy)
                {
                    case 0x01:
                    case 0x02:
                    case 0x64:
                        break;
                    default:
                        fprintf(stderr, "unexpected thingy %"PRIu32"\n", thingy);
                        exit(EXIT_FAILURE);
                }

                /* 0E: format? */
                expect32(0x01200101, infile);

                /* 12: ? */
                expect16(0x7D01, infile);

                /* 14: song id 2 */
                /* we will expect the program to use this same id, so it is
                  likely the program id */
                const uint32_t new_id2 = get32(infile);
                if (csf_no > 0)
                {
                    if (new_id2 != song_id2+1)
                    {
                        fprintf(stderr, "expected new song id2 0x%"PRIx32", got 0x%"PRIx32"\n",
                            song_id2+1, new_id2);
                        exit(EXIT_FAILURE);
                    }
                }
                song_id2 = new_id2;

                /* rest is probably a sequence that plays one sample? */
                get32(infile);      /* 18: duration? */
                expect32(0, infile);/* 1C */
                expect32(4, infile);/* 20 */
                expect16(0xC000, infile); /* 24 */
                get16(infile);      /* 26 */
                get32(infile);      /* 28 */
                get32(infile);      /* 2C */
            } /* end SONG chunk */
        } /* end BOOK chunk */

        /* PGHD chunk (set of programs) */
        {
            expect32(PGHD_magic, infile);   /* 00: 'PGHD' */
            expect32(0x70, infile);         /* 04: size */
            expect32(0x1, infile);          /* 08: program count? */
            expect32(0, infile);            /* 0C: padding? */

            /* PROG chunk (program) */
            {
                expect32(PROG_magic, infile);   /* 00: 'PROG' */
                expect32(0x60, infile);         /* 04: size */
                expect32(0x1, infile);          /* 08: sample count */
                expect32(song_id2, infile);     /* 0C: program id? */

                /* TIM chunk (sample?) */
                {
                    expect32(TIM_magic, infile);    /* 00: 'TIM' */
                    expect32(0x50, infile);         /* 04: size */
                    expect32(UINT32_C(0xFB000000), infile); /* 08: float? */
                    expect32(0, infile);    /* 0C */
                    expect32(0, infile);    /* 10 */
                    expect32(127, infile);  /* 14 */
                    expect16(127, infile);  /* 18 */
                    expect16((64<<8) | 64, infile); /* 1A */
                    expect32(2, infile);    /* 1C */

                    /* 20: traditional nibble count */
                    const uint32_t nibble_count = get32(infile);
                    get16(infile);          /* 24: initial p/s */
                    expect16(0, infile);    /* 26 */
                    expect32(0, infile);    /* 28 */
                    expect32(0, infile);    /* 2C */

                    decode_dsp(argv[1], song_id2, ftell(infile),
                            csf_offset+csf_header_size, nibble_count,
                            infile);
                } /* end TIM */
            } /* end PROG */
        } /* end PGHD */

        fflush(stdout);
    } /* end CSF loop */

    fclose(infile);
}

/* signed nibbles come up a lot */
int nibble_to_int[16] = {0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1};

int get_high_nibble_signed(uint8_t n) {
    /*return ((n&0x70)-(n&0x80))>>4;*/
    return nibble_to_int[n>>4];
}

int get_low_nibble_signed(uint8_t n) {
    /*return (n&7)-(n&8);*/
    return nibble_to_int[n&0xf];
}

int clamp16(int32_t val) {
        if (val>32767) return 32767;
            if (val<-32768) return -32768;
                return val;
}

int32_t dsp_nibbles_to_samples(int32_t nibbles) {
    int32_t whole_frames = nibbles/16;
    int32_t remainder = nibbles%16;

    if (remainder>=2) return whole_frames*14+remainder-2;
    else
    {
        fprintf(stderr, "bad nibble count %"PRId32"\n", nibbles);
        exit(EXIT_FAILURE);
    }
}

void decode_dsp(char * basename, uint32_t id, uint32_t coef_offset,
    uint32_t data_offset, uint32_t nibble_count, FILE * infile)
{
    int16_t coefs[8][2];
    FILE * outfile;
    uint32_t total_samples = 0;

    /* build output name */
    {
        size_t namelen = strlen(basename) + (1+8+1+3+1);
        char * name = malloc(namelen);
        snprintf(name, namelen, "%s_%08"PRIx32".wav", basename, id);

        outfile = fopen(name, "wb");
        if (!outfile)
        {
            fprintf(stderr, "error opening %s for output\n", name);
            exit(EXIT_FAILURE);
        }

        /* announce! */
        printf("    %s: %"PRId32" samples (%"PRIu32" nibbles)\n",
            name, dsp_nibbles_to_samples(nibble_count), nibble_count);

        free(name);
    }

    /* output wav header */
    {
        uint8_t wav_buf[0x2c];
        make_wav_header(wav_buf, dsp_nibbles_to_samples(nibble_count),
            22050, 1);
        if (0x2c != fwrite(wav_buf, 1, 0x2c, outfile))
        {
            fprintf(stderr, "error writing wav header\n");
            exit(EXIT_FAILURE);
        }
    }

    /* load coeffs */
    seek(coef_offset, infile);
    for (unsigned int i = 0; i < 8; i++)
    {
        coefs[i][0] = (int16_t)get16(infile);
        coefs[i][1] = (int16_t)get16(infile);
        //printf("%"PRId16",%"PRId16"\n",coefs[i][0],coefs[i][1]);
    }
    
    /* frame loop */
    seek(data_offset, infile);
    int32_t hist1 = 0, hist2 = 0;
    for (unsigned int s = 2; s <= nibble_count; s+=16)
    {
        uint8_t frame[8];
        int16_t outbuf[14];
        if (8 != fread(frame, 1, 8, infile))
        {
            fprintf(stderr, "error reading DSP data at 0x%lx\n",
                    (unsigned long)ftell(infile));
            exit(EXIT_FAILURE);
        }

        const int8_t header = frame[0];
        const int scale = 1 << (header & 0xf);
        const int coef_index = (header >> 4) & 0xf;
        const int16_t coef1 = coefs[coef_index][0];
        const int16_t coef2 = coefs[coef_index][1];

        int i;
        for (i=0; i<14 && s+i<nibble_count; i++) {
            const uint8_t sample_byte = frame[1+i/2];

            outbuf[i] = clamp16((
                 (((i&1?
                    get_low_nibble_signed(sample_byte):
                    get_high_nibble_signed(sample_byte)
                   ) * scale)<<11) + (1<<10) +
                 (coef1 * hist1 + coef2 * hist2))>>11
                );

            hist2 = hist1;
            hist1 = outbuf[i];
        }

        if (i != fwrite(outbuf, 2, i, outfile))
        {
            fprintf(stderr, "error writing PCM data\n");
            exit(EXIT_FAILURE);
        }
        total_samples += i;
    }

    if (total_samples != dsp_nibbles_to_samples(nibble_count))
    {
        fprintf(stderr, "decode mismatch, expected %"PRId32" samples, decoded "
                "%"PRId32"\n",
                dsp_nibbles_to_samples(nibble_count), total_samples);
        exit(EXIT_FAILURE);
    }

    if (EOF == fclose(outfile))
    {
        fprintf(stderr, "error closing output .wav\n");
        exit(EXIT_FAILURE);
    }
}

void put_32bitLE(uint8_t * buf, uint32_t v)
{
    for (unsigned int i = 0; i < 4; i++)
    {
        buf[i] = v & 0xFF;
        v >>= 8;
    }
}

void put_16bitLE(uint8_t * buf, uint16_t v)
{
    for (unsigned int i = 0; i < 2; i++)
    {
        buf[i] = v & 0xFF;
        v >>= 8;
    }
}

/* make a header for 16-bit PCM .wav */
/* buffer must be 0x2c bytes */
void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels) {
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

