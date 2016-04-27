#include <stdlib.h>
#include <stdio.h>
#include "error_stuff.h"
#include "util.h"

/* MPEG header info from http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm */

struct mpeg_header
{
    unsigned version        : 2;
    unsigned layer          : 2;
    unsigned crc_missing    : 1;
    unsigned bitrate_idx    : 4;
    unsigned samplerate_idx : 2;
    unsigned padding        : 1;
    unsigned private        : 1;
    unsigned channel_mode   : 2;
    unsigned js_mode_ext    : 2;
    unsigned copyrighted    : 1;
    unsigned original       : 1;
    unsigned emphasis       : 2;
};

/* return 0 on success, -1 on failure */
int load_header(struct mpeg_header *h, const uint8_t *h_in)
{
    memset(h, 0, sizeof(*h));

    /* 31-21: check for sync */
    if (h_in[0] != 0xff || (h_in[1] & 0xe0) != 0xe0)
    {
        return -1;
    }

    /* 20,19: version */
    h->version = (h_in[1] & 0x18) >> 3;

    /* 18,17: layer */
    h->layer = (h_in[1] & 0x06) >> 1;

    /* 16: protected by crc? */
    h->crc_missing = (h_in[1] & 0x01);

    /* 15-12: bitrate index */
    h->bitrate_idx = (h_in[2] & 0xf0) >> 4;

    /* 11,10: sampling rate frequency index */
    h->samplerate_idx = (h_in[2] & 0x0c) >> 2;

    /* 9: padding? */
    h->padding = (h_in[2] & 0x02) >> 1;

    /* 8: private bit */
    h->private = (h_in[2] & 0x01);

    /* 7,6: channel mode */
    h->channel_mode = (h_in[3] & 0xc0) >> 6;

    /* 5,4: mode extension for joint stereo */
    h->js_mode_ext = (h_in[3] & 0x30) >> 4;

    /* 3: copyrighted */
    h->copyrighted = (h_in[3] & 0x08) >> 3;

    /* 2: original */
    h->original = (h_in[3] & 0x04) >> 2;

    /* 1,0: emphasis */
    h->emphasis = (h_in[3] & 0x03);

    return 0;
}

struct mpeg_frame_info
{
    unsigned version;
    unsigned layer;
    unsigned bitrate;
    unsigned samplerate;
    unsigned frame_size;    /* samples */
    unsigned frame_length;  /* bytes */
    unsigned channels;
    char crc_present, private_bit, copyrighted, original;
};

/* return 0 for valid header, -1 for invalid */
int decode_header(struct mpeg_frame_info *f, const struct mpeg_header *h)
{
    memset(f, 0, sizeof(*f));

    if (1 == h->version || 0 == h->layer)
    {
        /* reserved */
        return -1;
    }

    f->version = h->version;
    f->layer = h->layer;

    int bitrate;

    /* look up bitrate */
    switch (f->version)
    {
        case 0: /* Version 2.5 */
        {
            /* not dealing with V2.5 now */
            return -1;
        }
        case 2: /* Version 2 */
        {
            static const int v2_bitrates[4][16] =
            { {}, /* reserved */
              { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96,  112, 128, 144, 160, -1 }, /* Layer III */
              { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96,  112, 128, 144, 160, -1 }, /* Layer II */
              { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, -1 }  /* Layer I */ };
            bitrate = v2_bitrates[h->layer][h->bitrate_idx];
            break;
        }
        case 3: /* Version 1 */
        {
            static const int v1_bitrates[4][16] =
            { {},   /* reserved */
              { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1 }, /* Layer III */
              { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1 }, /* Layer II */
              { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1 }  /* Layer I */ };

            bitrate = v1_bitrates[h->layer][h->bitrate_idx];
            break;
        }
        default:
        {
            return -1;
        }
    }

    if (-1 == bitrate || 0 == bitrate)
    {
        /* reject invalid or freeform */
        return -1;
    }

    f->bitrate = bitrate;

    int samplerate;
    /* look up sampling rate */
    switch (h->version)
    {
        case 0: /* Version 2.5 */
        {
            return -1;
        }
        case 2: /* Version 2 */
        {
            static const int v2_samplerates[4] = {22050, 24000, 16000, -1};

            samplerate = v2_samplerates[h->samplerate_idx];
            break;
        }
        case 3: /* Version 1 */
        {
            static const int v1_samplerates[4] = {44100, 48000, 32000, -1};

            samplerate = v1_samplerates[h->samplerate_idx];
            break;
        }
        default:
        {
            return -1;
        }
    }

    if (-1 == samplerate)
    {
        /* reject reserved */
        return -1;
    }

    f->samplerate = samplerate;

    /* look up frame size */
    static const int frame_sizes[4] = { -1, 1152, 1152, 384 };
    f->frame_size = frame_sizes[h->layer];

    /* calculate frame length */
    int frame_length;
    switch (h->layer)
    {
        case 3: /* Layer I */
            frame_length = (12l * bitrate * 1000l / samplerate + h->padding) * 4;
            break;
        case 1: /* Layer III */
        case 2: /* Layer II */
            frame_length = 144l * bitrate * 1000l / samplerate + h->padding;
            break;
        default:
            return -1;
    }


    f->frame_length = frame_length;

    /* MPEG-2 Layer III special case */
    if ( 2 == h->version && 1 == h->layer )
    {
        f->frame_size = 576;
        f->frame_length = 72l * bitrate * 1000l / samplerate + h->padding;
    }

    switch (h->channel_mode)
    {
        case 0: /* Stereo */
        case 1: /* Joint stereo */
        case 2: /* Dual channel */
            f->channels = 2;
            break;
        case 3: /* Single channel */
            f->channels = 1;
            break;
    }

    f->crc_present = !h->crc_missing;
    f->copyrighted = h->copyrighted;
    f->private_bit = h->private;
    f->original = h->original;

    return 0;
}

void describe_header(const struct mpeg_frame_info *f, FILE *outfile)
{
    static const char *version_strs[4] = {"2.5", "reserved", "2", "1"};
    static const char *layer_strs[4] = {NULL, "III", "II", "I"};

    fprintf(outfile, "MPEG Version %s Layer %s %u kbps %u Hz %s\n",
            version_strs[f->version], layer_strs[f->layer], f->bitrate,
            f->samplerate, ( 2 == f->channels ? "stereo" : "mono" ) );

    fprintf(outfile, "Frame is %u samples, %u bytes\n", f->frame_size,
            f->frame_length);

    const char *sep = "Flags: ";
    if (f->crc_present)
    {
        fprintf(outfile, "%sCRC present", sep);
        sep = ", ";
    }
    if (f->private_bit)
    {
        fprintf(outfile, "%sPrivate Bit Set", sep);
        sep = ", ";
    }
    if (f->copyrighted)
    {
        fprintf(outfile, "%sCopyrighted", sep);
        sep = ", ";
    }
    if (f->original)
    {
        fprintf(outfile, "%sOriginal", sep);
        sep = ", ";
    }


    printf("\n");
}

void reverse_nibbles(FILE *infile, long offset, size_t size)
{
    unsigned char buf[0x800];

    while (size > 0)
    {
        size_t bytes_to_fix = sizeof(buf);
        if (bytes_to_fix > size) bytes_to_fix = size;

        CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

        size_t bytes_read = fread(buf, 1, bytes_to_fix, infile);
        //fprintf(stderr,"offset=%lx bytes_to_fix=%lx bytes_read=%lx\n",offset,(long)bytes_to_fix,(long)bytes_read);
        CHECK_FILE(bytes_read != bytes_to_fix, infile, "fread");

        for (int i = 0; i < bytes_to_fix; i++)
        {
            buf[i] = ((buf[i] & 0xf0) >> 4) | ((buf[i] & 0x0f) << 4);
        }

        CHECK_ERRNO(fseek(infile, offset, SEEK_SET) != 0, "fseek");

        size_t bytes_written = fwrite(buf, 1, bytes_to_fix, infile);
        CHECK_FILE(bytes_written != bytes_to_fix, infile, "fwrite");

        size -= bytes_to_fix;
        offset += bytes_to_fix;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr,"usage: mpeg_nibble_reverse file.WAV\n");
        exit(EXIT_FAILURE);
    }

    FILE *infile = fopen(argv[1], "r+b");
    CHECK_ERRNO(!infile, "fopen");

    CHECK_ERRNO(fseek(infile, 0, SEEK_END) == -1, "fseek");
    const long file_size = ftell(infile);
    CHECK_ERRNO(file_size == -1, "ftell");

    // first frame is not swapped
    struct mpeg_header mpeg_header;
    uint8_t mpeg_header_buf[4];
    CHECK_ERRNO(fseek(infile, 0, SEEK_SET) == -1, "fseek");
    CHECK_FILE(fread(mpeg_header_buf, 4, 1, infile) != 1, infile, "fread header");
    CHECK_ERROR(load_header(&mpeg_header, mpeg_header_buf) != 0, "load_header");
    struct mpeg_frame_info mpeg_frame_info;
    CHECK_ERROR(decode_header(&mpeg_frame_info, &mpeg_header) != 0, "decode_header");

    describe_header(&mpeg_frame_info, stdout);

    const int skip_size = mpeg_frame_info.frame_length;

    reverse_nibbles(infile, skip_size, file_size-skip_size);
}
