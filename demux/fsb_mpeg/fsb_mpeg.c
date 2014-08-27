#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

/* fsb_mpeg 0.12 */

#define CHECK(x,msg) \
    do { \
        if (x) { \
            fflush(stdout); \
            fprintf(stderr, "error " msg "\n"); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define CHECK_ERRNO(x,msg) \
    do { \
        if (x) { \
            fflush(stdout); \
            perror("error " msg); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define CHECK_FILE(x,file,msg) \
    do { \
        if (x) { \
            int temperrno = errno; \
            fflush(stdout); \
            if (feof(file)) { \
                fprintf(stderr, "error " msg ": eof\n"); \
            } \
            else \
            { \
                errno = temperrno; \
                perror(msg); \
            } \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static long gPadding = 16;
static long gBodyPadding = 0;
static int gPadding_is_max = 0;

void unpack_mpeg(FILE *infile, char *name_base, long start_offset, long end_offset, int stream_count, long expected_samples);

static inline int32_t read32bitLE(unsigned char *buf)
{
    int i;
    uint32_t value = 0;

    for ( i = 3; i >= 0; i-- )
    {
        value <<= 8;
        value |= buf[i];
    }

    return value;
}

static inline int16_t read16bitLE(unsigned char *buf)
{
    int i;
    uint32_t value = 0;

    for ( i = 1; i >= 0; i-- )
    {
        value <<= 8;
        value |= buf[i];
    }

    return value;
}

static inline void write32bitLE(int32_t value, unsigned char *buf)
{
    int i;
    uint32_t v = (uint32_t)value;

    for ( i = 0; i < 4; i++ )
    {
        buf[i] = v & 0xff;
        v >>= 8;
    }
}

void copy_bytes(FILE *infile, FILE *outfile, long offset, long count)
{
    int rc;
    size_t size_rc;
    unsigned char buf[0x800];

    rc = fseek(infile, offset, SEEK_SET);
    CHECK_ERRNO(rc == -1, "seek for copy");

    while (count > 0)
    {
        long dump_size = (count > sizeof(buf) ? sizeof(buf) : count);

        size_rc = fread(buf, 1, dump_size, infile);
        CHECK_FILE(size_rc != dump_size, infile, "reading for copy");

        size_rc = fwrite(buf, 1, dump_size, outfile);
        CHECK_FILE(size_rc != dump_size, outfile, "writing for copy");

        count -= dump_size;
    }
}

const char fsb3headmagic[4]="FSB3"; /* not terminated */
const char fsb4headmagic[4]="FSB4"; /* not terminated */

const int fsb3headsize = 0x18;
const int fsb4headsize = 0x30;
const int maxheadsize = 0x30;

enum fsb_type_t {
    fsb3, fsb4
};

/* 1 if it seems to be a valid header, 0 otherwise */
int test_fsb_header(unsigned char *header, int32_t *size)
{
    int32_t header_size;
    int32_t table_size;
    int32_t body_size;
    int32_t stream_count;

    /* check magic */
    if (!memcmp(&header[0],fsb3headmagic,4))
    {
        header_size = fsb3headsize;
    }
    else if (!memcmp(&header[0],fsb4headmagic,4))
    {
        header_size = fsb4headsize;
    }
    else
    {
        return 0;
    }

    /* check for positive stream count */
    stream_count = read32bitLE(&header[4]);
    if (stream_count <= 0) return 0;

    /* check for positive table size */
    table_size = read32bitLE(&header[8]);
    if (table_size <= 0) return 0;

    /* check for positive body size */
    body_size = read32bitLE(&header[12]);
    if (body_size <= 0) return 0;

    uint64_t total_size = (uint64_t)header_size +
        (uint64_t)table_size +
        (uint64_t)body_size;

    /* looks potentially valid */
    *size = total_size;
    return 1;
}

int try_multistream_fsb(FILE *infile)
{
    int32_t stream_count;
    int32_t table_size;
    int32_t body_size;
    long whole_file_size;
    int32_t header_size;
    size_t size_rc;
    int rc;
    unsigned char header[maxheadsize];
    enum fsb_type_t fsb_type;

    /* get file size */
    {
        rc = fseek(infile, 0, SEEK_END);
        CHECK_ERRNO(rc == -1, "seeking to file end");

        whole_file_size = ftell(infile);
        CHECK_ERRNO(whole_file_size == -1, "getting file size");
    }

    /* read header */
    {
        rc = fseek(infile, 0, SEEK_SET);
        CHECK_ERRNO(rc == -1, "seeking to file start");

        size_rc = fread(header, 1, 4, infile);
        CHECK_FILE(size_rc != 4, infile, "reading magic");

        if (!memcmp(&header[0],fsb3headmagic,4))
        {
            printf("Type: FSB3\n");
            header_size = fsb3headsize;
            fsb_type = fsb3;
        }
        else if (!memcmp(&header[0],fsb4headmagic,4))
        {
            printf("Type: FSB4\n");
            header_size = fsb4headsize;
            fsb_type = fsb4;
        }
        else
        {
            /* couldn't find a valid multistream fsb to unpack */
            return 0;
        }

        /* read the rest of the header */
        size_rc = fread(&header[4], 1, header_size-4, infile);
        CHECK_FILE(size_rc != header_size-4, infile, "reading header");

        stream_count = read32bitLE(&header[4]);
        CHECK(stream_count <= 0, "bad stream count");

        table_size = read32bitLE(&header[8]);
        CHECK(table_size <= 0, "bad table size");
        body_size = read32bitLE(&header[12]);
        CHECK(body_size <= 0, "bad body size");

        printf("Header: 0x%" PRIx32 " bytes\n", (uint32_t)header_size);
        printf("Table:  0x%" PRIx32 " bytes\n", (uint32_t)table_size);
        printf("Body:   0x%" PRIx32 " bytes\n", (uint32_t)body_size);
        printf("------------------\n");

        uint64_t total_size = (uint64_t)header_size +
                (uint64_t)table_size +
                (uint64_t)body_size;
        printf("Total:  0x%" PRIx64 " bytes\n", total_size);
        printf("File:   0x%lx bytes\n", whole_file_size);

        CHECK( whole_file_size < total_size ,
                "file size less than FSB size, truncated?");
#if 0
        if ( whole_file_size - total_size > 0x800 )
        {
            /* maybe embedded? */
            return 0;
        }
#endif

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
            static const char mp3ext[] = ".mp3";
            const int entry_min_size = 0x40;
            unsigned char entry_buf[0x40];
            char name_buf[0x1e + 1];
            char *name_base = NULL;
            FILE *outfile;

            rc = fseek(infile, table_offset, SEEK_SET);
            CHECK_ERRNO(rc, "seeking to table entry");

            size_rc = fread(entry_buf, 1, entry_min_size, infile);
            CHECK_FILE(size_rc != entry_min_size, infile,
                    "reading table entry header");

            entry_size = read16bitLE(&entry_buf[0]);
            CHECK(entry_size < entry_min_size, "entry too small");
            padding_size = 0x10 - (header_size + entry_size) % 0x10;

            entry_file_size = read32bitLE(&entry_buf[0x24]);

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
            memcpy(name_buf, entry_buf+2, 0x1e);

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

            /* get channel count */
            uint16_t channels = read16bitLE(&entry_buf[0x3e]);
            int streams = (channels + 1) / 2;

            /* get sample count */
            uint32_t samples = read32bitLE(&entry_buf[0x20]);

            printf("%"PRIu16" channel%s (%d streams), %"PRIu32" samples\n",
                    channels, (1==channels?"":"s"), streams, samples);

            /* write out files */
            unpack_mpeg(infile, name_base, body_offset, body_offset + entry_file_size, streams, samples);

            printf("\n");

            /* clean up name_base string */
            free(name_base);
            name_base = NULL;

            /* onward! */
            table_offset += entry_size;
            body_offset += entry_file_size + body_padding_size;
        }
    }

    return 1;
}

void usage(void)
{
    fprintf(stderr, "fsb_mpeg 0.12 by hcs\n"
                    "usage: fsb_mpeg file.fsb [-p N] [-b N]\n"
                    " -p N: assume up to N bytes of padding per frame\n"
                    " -b N: assume that streams are padded to N bytes\n");
}

int main(int argc, char *argv[])
{
    FILE *infile;
    int rc;
    if (argc < 2)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    for (int i = 2; i < argc; i++)
    {
        if (!strcmp(argv[i],"-p"))
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "missing value for -p\n");
                usage();
                exit(EXIT_FAILURE);
            }
            gPadding = strtol(argv[i], NULL, 10);
            gPadding_is_max = 1;
            if (0 == gPadding)
            {
                fprintf(stderr, "invalid padding value\n");
                usage();
                exit(EXIT_FAILURE);
            }
        }
        else if (!strcmp(argv[i],"-b"))
        {
            i++;
            if (i >= argc)
            {
                fprintf(stderr, "missing value for -b\n");
                usage();
                exit(EXIT_FAILURE);
            }
            gBodyPadding = strtol(argv[i], NULL, 10);
            if (0 == gBodyPadding)
            {
                fprintf(stderr, "invalid body padding value\n");
                usage();
                exit(EXIT_FAILURE);
            }

        }
        else
        {
            usage();
            exit(EXIT_FAILURE);
        }
    }

    infile = fopen(argv[1],"rb");
    CHECK_ERRNO(infile == NULL, "opening input");

    if (!try_multistream_fsb(infile))
    {
        printf("Sorry, couldn't make any sense of this file.\n");
        exit(EXIT_FAILURE);
    }

    rc = fclose(infile);
    CHECK_ERRNO(rc != 0, "closing input file");

    printf("Success!\n");

    exit(EXIT_SUCCESS);
}

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


void unpack_mpeg(FILE *infile, char *name_base, long start_offset, long end_offset, int stream_count, long expected_samples)
{
    struct mpeg_header header;
    struct mpeg_frame_info info;

    uint8_t header_buf[4];

    long *sample_totals = NULL;
    FILE **outfiles = NULL;
    char **outfile_names;

    outfiles = malloc(sizeof(FILE*) * stream_count);
    CHECK_ERRNO(!outfiles, "malloc");
    for (int i = 0 ; i < stream_count; i++)
    {
        outfiles[i] = NULL;
    }

    outfile_names = malloc(sizeof(char*) * stream_count);
    CHECK_ERRNO(!outfile_names, "malloc");
    for (int i = 0; i < stream_count; i++)
    {
        outfile_names[i] = NULL;
    }

    /* name and open output files */
    {
        size_t numberlen = ceil(log10(stream_count+2)); /* +1 because we add 1, +1 to round up even counts */
        /* "name_number.mp3\0" */
        size_t namelen = strlen(name_base) + 1 + numberlen + 1 + 3 + 1;
        for (int i = 0; i < stream_count; i++)
        {
            outfile_names[i] = malloc(namelen);
            CHECK_ERRNO(!outfile_names[i], "malloc");

            snprintf(outfile_names[i], namelen, "%s_%0*u.mp3", name_base,
                    (int)numberlen, (unsigned int)(i+1));
            outfiles[i] = fopen(outfile_names[i], "wb");
            CHECK_ERRNO(!outfiles[i], "fopen");
        }
    }

    sample_totals = malloc(sizeof(long) * stream_count);
    CHECK_ERRNO(!sample_totals, "malloc");
    for (int i = 0; i < stream_count; i++)
    {
        sample_totals[i] = 0;
    }

    /* check for a valid MPEG frame */
    CHECK_ERRNO(-1 == fseek(infile, start_offset, SEEK_SET), "fseek");
    CHECK_FILE(4 != fread(header_buf, 1, 4, infile), infile,
                "bad read looking for sync");

    if (-1 == load_header(&header, header_buf))
    {
        printf("didn't find a valid MPEG frame sync\n");
        goto done;
    }
    if (-1 == decode_header(&info, &header))
    {
        printf("didn't find a valid MPEG frame\n");
        goto done;
    }

    describe_header(&info, stdout);

    for (int i = 0; i < stream_count; i++)
    {
        printf("%s\n", outfile_names[i]);
    }

    /* follow the streams */
    int cur_stream = 0;
    long offset = start_offset;
    long last_pad = 0;
    while (offset < end_offset)
    {
        int load_rc;

        // Bioshock 2 especially has weird situations where it doesn't do
        // padding consistently throughout the file, so we take the padding
        // as a maximum and try finding syncs at or before that.
        {
            long initial_offset = offset;

            for ( ; last_pad >= 0; last_pad --)
            {
                if ( offset + 4 <= end_offset )
                {
                    CHECK_ERRNO(-1 == fseek(infile, offset, SEEK_SET), "fseek");
                    CHECK_FILE(4 != fread(header_buf, 1, 4, infile), infile, "fread");

                    load_rc = load_header(&header, header_buf);
                }
                else
                {
                    // avoid attempting to read past end of file
                    load_rc = -1;
                }

                if (0 == load_rc || !gPadding_is_max)
                {
                    break;
                }

                offset --;
            }

            if (-1 == load_rc)
            {
                // if we've totally lost sync, undo the pad adjustment
                offset = initial_offset;
            }
        }

        if (-1 == load_rc)
        {
            printf("lost sync at 0x%lx (file ends at 0x%lx)\n",
                    (unsigned long)offset, (unsigned long)end_offset);
            break;
        }
        if (-1 == decode_header(&info, &header))
        {
            printf("bad MPEG header at 0x%lx (file ends at 0x%lx)\n",
                    (unsigned long)offset, (unsigned long)end_offset);
            break;
        }
        int rounded_length = (info.frame_length + gPadding-1) / gPadding * gPadding;

        last_pad = rounded_length - info.frame_length;

        copy_bytes(infile, outfiles[cur_stream], offset, info.frame_length);

        sample_totals[cur_stream] += info.frame_size;

        cur_stream = (cur_stream + 1) % stream_count;
        offset += rounded_length;
    }

    for (int i = 0; i < stream_count; i++)
    {
        //printf("sample_totals[%d] = %d, expected_samples = %d\n", i, sample_totals[i], expected_samples);

        // Bioshock 2 bug with MPEG-2 Layer III with only 2 frames, or
        // MPEG-1 Layer III with only 1 frame. I guess the bug involves 1152
        // samples somehow, something counts another frame (or leaves one out
        // of the FSB).
        if (sample_totals[i] == 1152 &&
            ((info.version == 2 && info.layer == 1 && expected_samples == 1728) ||
             (info.version == 3 && info.layer == 1 && expected_samples == 2304)))
        {
            // known bug, skip the check
            continue;
        }

        CHECK(sample_totals[i] != expected_samples, "sample count mismatch");
    }

done:
    if (outfile_names)
    {
        for (int i = 0; i < stream_count; i++)
        {
            free(outfile_names[i]);
        }

        free(outfile_names);
    }

    if (outfiles)
    {
        for (int i = 0; i < stream_count; i++)
        {
            CHECK_ERRNO(EOF == fclose(outfiles[i]), "fclose");
        }
        free(outfiles);
    }
    
    if (sample_totals)
    {
        free(sample_totals);
    }
}
