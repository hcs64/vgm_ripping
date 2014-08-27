#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

#include "error_stuff.h"
#include "util.h"

/* Revolution B - brstm building and extraction tool */
/* by hcs (http://here.is/halleyscomet) */

/* NOTE: I'm not clear on what should happen if the byte count is an exact
 * multiple of the size of a block. At the moment I assume that the
 * last block size will be set to 0 and the block count will include all
 * complete blocks.
 */

/* NOTE: The ADPC chunk is created but is not filled with anything. In theory
 * I can decode the DSP and fill in these values (assuming that I understand its
 * purpose correctly), but this is not yet done so seeking may be a bit broken.
 */

#define VERSION "0.4"
#define MAX_CHANNELS 8

enum odd_options
{
    OPTION_SECOND_CHUNK_EXTRA = 1,
    OPTION_ALTERNATE_ADPC_COUNT = 2,
};

enum revb_mode
{
    MODE_INVALID,
    MODE_BUILD,
    MODE_EXTRACT,
    MODE_EXAMINE,
};

static void extract(const char *brstm_name, const char *dsp_names [],
        int dsp_count);
static void build(const char *brstm_name, const char *dsp_names [],
        int dsp_count, enum odd_options options);
static uint32_t samples_to_nibbles(uint32_t samples);
static uint32_t nibbles_to_samples(uint32_t nibbles);

static void expect_8(uint8_t expected, long offset, const char *desc,
        FILE *infile);
static void expect_16(uint16_t expected, long offset, const char *desc,
        FILE *infile);
static void expect_32(uint32_t expected, long offset, const char *desc,
        FILE *infile);

static const char *bin_name = NULL;

static void usage(void)
{
    fprintf(stderr,
            "Revolution B\n"
            "Version " VERSION " (built " __DATE__ ")\n\n"
            "Build .brstm files from mono .dsp (or extract)\n"
            "examine usage:\n"
            "    %s --examine source.brstm\n"
            "build usage:\n"
            "    %s --build dest.brstm source.dsp [sourceR.dsp%s] [options]\n"
            "extract usage:\n"
            "    %s --extract source.brstm dest.dsp [destR.dsp%s]\n"
            "build options:\n"
            "  --second-chunk-extra\n"
            "  --alternate-adpc-count\n"
            "\n",
            bin_name, bin_name, (MAX_CHANNELS > 2 ? " ..." : ""), bin_name,
            (MAX_CHANNELS > 2 ? " ..." : ""));

    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    const char *brstm_name = NULL;
    const char *dsp_names[MAX_CHANNELS];
    enum revb_mode mode = MODE_INVALID;
    enum odd_options options = 0;
    int dsp_count = 0;

    /* for usage() */
    bin_name = argv[0];

    /* clear dsp_names */
    for (int i = 0; i < MAX_CHANNELS; i++) dsp_names[i] = NULL;

    /* process arguments */
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp("--extract", argv[i]))
        {
            if (mode != MODE_INVALID) usage();
            if (i >= argc-1) usage();

            mode = MODE_EXTRACT;
            brstm_name = argv[++i];
        }
        else if (!strcmp("--build", argv[i]))
        {
            if (mode != MODE_INVALID) usage();
            if (i >= argc-1) usage();

            mode = MODE_BUILD;
            brstm_name = argv[++i];
        }
        else if (!strcmp("--examine", argv[i]))
        {
            if (mode != MODE_INVALID) usage();
            if (i >= argc-1) usage();

            mode = MODE_EXAMINE;
            brstm_name = argv[++i];
        }
        else if (!strcmp("--second-chunk-extra", argv[i]))
        {
            if (options & OPTION_SECOND_CHUNK_EXTRA) usage();
            options |= OPTION_SECOND_CHUNK_EXTRA;
        }
        else if (!strcmp("--alternate-adpc-count", argv[i]))
        {
            if (options & OPTION_ALTERNATE_ADPC_COUNT) usage();
            options |= OPTION_ALTERNATE_ADPC_COUNT;
        }
        else
        {
            if (dsp_count == MAX_CHANNELS)
            {
                fprintf(stderr,"Maximum of %d channels.\n",MAX_CHANNELS);
                exit(EXIT_FAILURE);
            }
            dsp_names[dsp_count++] = argv[i];
        }
    }

    /* some additional mode checks */
    if (mode != MODE_BUILD && options != 0) usage();
    if (mode == MODE_EXAMINE && dsp_count != 0) usage();
    if (mode != MODE_EXAMINE && dsp_count == 0) usage();

    switch (mode)
    {
        case MODE_BUILD:
            if (dsp_count > 2)
            {
                fprintf(stderr, "Not comfortable with building > 2 channels\n");
                exit(EXIT_FAILURE);
            }
            build(brstm_name, dsp_names, dsp_count, options);
            break;
        case MODE_EXAMINE:
        case MODE_EXTRACT:
            extract(brstm_name, dsp_names, dsp_count);
            break;
        case MODE_INVALID:
            usage();
            break;
    }

    exit(EXIT_SUCCESS);
}

static void expect_8(uint8_t expected, long offset, const char *desc,
        FILE *infile)
{
    uint8_t found = get_byte_seek(offset, infile);
    if (found != expected)
    {
        fprintf(stderr,"expected 0x%02"PRIx8" at offset 0x%lx (%s), "
                "found 0x%02"PRIx8"\n",
                expected, (unsigned long)offset, desc, found);
        exit(EXIT_FAILURE);
    }
}
static void expect_16(uint16_t expected, long offset, const char *desc,
        FILE *infile)
{
    uint16_t found = get_16_be_seek(offset, infile);
    if (found != expected)
    {
        fprintf(stderr,"expected 0x%04"PRIx16" at offset 0x%lx (%s), "
                "found 0x%04"PRIx16"\n",
                expected, (unsigned long)offset, desc, found);
        exit(EXIT_FAILURE);
    }
}
static void expect_32(uint32_t expected, long offset, const char *desc,
        FILE *infile)
{
    uint32_t found = get_32_be_seek(offset, infile);
    if (found != expected)
    {
        fprintf(stderr,"expected 0x%08"PRIx32" at offset 0x%lx (%s), "
                "found 0x%08"PRIx32"\n",
                expected, (unsigned long)offset, desc, found);
        exit(EXIT_FAILURE);
    }
}

static uint32_t samples_to_nibbles(uint32_t samples)
{
    uint32_t nibbles = samples / 14 * 16;
    if (samples % 14) nibbles += 2 + samples % 14;
    return nibbles;
}

static uint32_t nibbles_to_samples(uint32_t nibbles)
{
    uint32_t whole_frames = nibbles / 16;
    if (nibbles % 16)
    {
        return whole_frames * 14 + nibbles % 16 - 2;
    }
    return whole_frames * 14;
}

static const uint8_t RSTM_sig[4] = {'R','S','T','M'};
static const uint8_t head_name[4] = {'H','E','A','D'};
static const uint8_t adpc_name[4] = {'A','D','P','C'};
static const uint8_t data_name[4] = {'D','A','T','A'};

static void extract(const char *brstm_name, const char *dsp_names [],
        int dsp_count)
{
    FILE *infile = NULL;
    long infile_size;
    FILE *outfiles[MAX_CHANNELS];

    int options = 0;
    int examine_mode = (dsp_count == 0);

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        outfiles[i] = NULL;
    }

    /* open input file */
    infile = fopen(brstm_name, "rb");
    CHECK_ERRNO(infile == NULL, "fopen of input file");

    CHECK_ERRNO(fseek(infile, 0, SEEK_END) == -1, "fseek");
    infile_size = ftell(infile);
    CHECK_ERRNO(infile_size == -1, "ftell");

    /* open output files */
    if (!examine_mode)
    {
        for (int i = 0; i < dsp_count; i++)
        {
            outfiles[i] = fopen(dsp_names[i], "wb");
            CHECK_ERRNO(outfiles[i] == NULL, "fopen of output file");
        }
    }

    /* announce intentions */
    if (examine_mode)
        fprintf(stderr, "Examining ");
    else
        fprintf(stderr, "Extracting ");
    fprintf(stderr, "%s", brstm_name);
    if (examine_mode)
        fprintf(stderr, "\n");
    else
    {
        fprintf(stderr, " to:\n");
        for (int i = 0; i < dsp_count; i++)
        {
            fprintf(stderr, "  channel %d: %s\n", i, dsp_names[i]);
        }
    }

    fprintf(stderr,"\n");

    /* read RSTM header */
    uint32_t rstm_size;
    uint32_t head_offset = 0, head_size = 0;
    uint32_t adpc_offset = 0, adpc_size = 0;
    uint32_t data_offset = 0, data_size = 0;
    {
        uint8_t buf[4];

        /* check signature */
        get_bytes_seek(0, infile, buf, 4);
        if (memcmp(RSTM_sig, buf, 4))
        {
            fprintf(stderr,"no RSTM header\n");
            exit(EXIT_FAILURE);
        }

        /* check version */
        expect_32(UINT32_C(0xFEFF0100), 4, "RSTM version", infile);

        /* read size */
        rstm_size = get_32_be_seek(8, infile);

        if (infile_size != rstm_size)
        {
            fprintf(stderr, "file size (%ld) != RSTM size (%"PRIu32")\n",
                    infile_size, rstm_size);
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "RSTM size = %"PRIx32"\n", rstm_size);

        /* check certain funky bits */
        expect_16(0x40, 0xc, "header size", infile);
        expect_16(0x2, 0xe, "??", infile);

        /* read chunk list */
        fprintf(stderr, "RSTM chunks:\n");
        for (int i = 0; i < (0x40-0x10)/8; i++)
        {
            uint32_t chunk_offset = get_32_be_seek(0x10+i*8, infile);
            uint32_t chunk_size = get_32_be_seek(0x10+i*8+4, infile);
            uint8_t chunk_name[5]={0};

            if (chunk_offset == 0) continue;

            get_bytes_seek(chunk_offset, infile, chunk_name, 4);
            expect_32(chunk_size, chunk_offset+4, "chunk size", infile);

            /* print chunk info */
            {
                int unprintable = 0;
                fprintf(stderr, "  chunk %d ", i);
                for (int i = 0; i < 4; i++)
                {
                    unprintable = unprintable || !isprint(chunk_name[i]);
                }
                if (unprintable)
                {
                    for (int i = 0;  i < 4; i++)
                    {
                        fprintf(stderr, "%02"PRIx8, chunk_name[i]);
                    }
                }
                else
                {
                    fprintf(stderr, "'%s'", chunk_name);
                }

                fprintf(stderr, " offset: 0x%08"PRIx32" size: 0x%08"PRIx32"\n",
                        chunk_offset, chunk_size);
            }

            /* handle individual chunk types */
            if (!memcmp(chunk_name, head_name, sizeof(head_name)))
            {
                head_offset = chunk_offset + 8;
                head_size = chunk_size - 8;
            }
            else if (!memcmp(chunk_name, adpc_name, sizeof(adpc_name)))
            {
                adpc_offset = chunk_offset + 8;
                adpc_size = chunk_size - 8;
            }
            else if (!memcmp(chunk_name, data_name, sizeof(data_name)))
            {
                data_offset = chunk_offset + 8;
                data_size = chunk_size - 8;
            }
            else
            {
                fprintf(stderr, "Unknown chunk type\n");
                exit(EXIT_FAILURE);
            }
        }

        if (head_offset == 0 || adpc_offset == 0 || data_offset == 0)
        {
            fprintf(stderr, "Didn't find HEAD, ADPC and DATA chunks\n");
            exit(EXIT_FAILURE);
        }
    }   /* end read RSTM header */

    /* scan HEAD chunk and generate DSP headers */
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t last_block_size;
    uint32_t last_block_used_bytes;
    uint32_t head_adpc_blocks;
    {
        /* chunk list */
        expect_32(0x01000000, head_offset + 0, "dummy size?", infile);
        uint32_t stream_info_offset =
            head_offset + get_32_be_seek(head_offset + 4, infile);
        expect_32(0x01000000, head_offset + 8, "dummy size?", infile);
        uint32_t second_chunk_offset = 
            head_offset + get_32_be_seek(head_offset + 0xc, infile);
        expect_32(0x01000000, head_offset + 0x10, "dummy size?", infile);
        uint32_t third_chunk_offset =
            head_offset + get_32_be_seek(head_offset + 0x14, infile);

        /* first chunk (stream info) */
        uint8_t codec_id =
            get_byte_seek(stream_info_offset + 0, infile);
        uint8_t loop_flag =
            get_byte_seek(stream_info_offset + 1, infile);
        uint8_t channel_count =
            get_byte_seek(stream_info_offset + 2, infile);
        expect_8(0, stream_info_offset + 3, "??", infile);
        uint16_t sample_rate =
            get_16_be_seek(stream_info_offset + 4, infile);
        expect_16(0, stream_info_offset + 6, "padding?", infile);
        uint32_t loop_start =
            get_32_be_seek(stream_info_offset + 8, infile);
        uint32_t sample_count =
            get_32_be_seek(stream_info_offset + 0xc, infile);
        uint32_t head_data_start =
            get_32_be_seek(stream_info_offset + 0x10, infile);
        total_blocks =
            get_32_be_seek(stream_info_offset + 0x14, infile);
        block_size =
            get_32_be_seek(stream_info_offset + 0x18, infile);
        uint32_t samples_per_block =
            get_32_be_seek(stream_info_offset + 0x1c, infile);
        last_block_used_bytes =
            get_32_be_seek(stream_info_offset + 0x20, infile);
        uint32_t last_block_samples =
            get_32_be_seek(stream_info_offset + 0x24, infile);
        last_block_size =
            get_32_be_seek(stream_info_offset + 0x28, infile);
        uint32_t samples_per_adpc_entry =
            get_32_be_seek(stream_info_offset + 0x2c, infile);
        uint32_t bytes_per_adpc_entry =
            get_32_be_seek(stream_info_offset + 0x30, infile);

        /* print stream info */
        fprintf(stderr, "Stream info:\n");
        {
            const char *codec_string = "unknown";
            fprintf(stderr, "  Codec:                 %d (", codec_id);
            switch (codec_id)
            {
                case 0:
                    codec_string = "8-bit PCM?";
                    break;
                case 1:
                    codec_string = "16-bit PCM";
                    break;
                case 2:
                    codec_string = "4-bit DSP ADPCM";
                    break;
            }
            fprintf(stderr, "%s)\n", codec_string);
        }
        fprintf(stderr, "  Loop flag:             %d (%s)\n", loop_flag,
                loop_flag?"looped":"non-looped");
        fprintf(stderr, "  Channels:              %d\n", channel_count);
        fprintf(stderr, "  Sample rate:           %"PRIu16" Hz\n", sample_rate);
        fprintf(stderr, "  Loop start:            %"PRIu32" samples\n",
                loop_start);
        fprintf(stderr, "  Total samples:         %"PRIu32"\n", sample_count);
        fprintf(stderr, "  Data start:          0x%"PRIx32"\n",
                head_data_start);
        fprintf(stderr, "  Block count:           %"PRIu32"\n", total_blocks);
        fprintf(stderr, "  Block size:          0x%"PRIx32"\n", block_size);
        fprintf(stderr, "  Samples per block:     %"PRIu32"\n",
                samples_per_block);
        fprintf(stderr, "  Last block used bytes: %"PRIu32"\n",
                last_block_used_bytes);
        fprintf(stderr, "  Last block samples:    %"PRIu32"\n",
                last_block_samples);
        fprintf(stderr, "  Last block size:       %"PRIu32"\n",
                last_block_size);
        fprintf(stderr, "  Samples per ADPC entry? %"PRIu32"\n",
                samples_per_adpc_entry);
        fprintf(stderr, "  Bytes per ADPC entry?   %"PRIu32"\n",
                bytes_per_adpc_entry);

        /* check stream info */
        if (codec_id != 2)
        {
            fprintf(stderr, "only DSP coding supported\n");
            exit(EXIT_FAILURE);
        }

        if (!examine_mode && channel_count != dsp_count)
        {
            fprintf(stderr,
                    "stream has %d channels, but %d DSPs were specified\n",
                    channel_count, dsp_count);
            exit(EXIT_FAILURE);
        }

        if (head_data_start !=
                data_offset + get_32_be_seek(data_offset, infile))
        {
            fprintf(stderr,
                    "HEAD data offset does not agree with DATA chunk\n");
            exit(EXIT_FAILURE);
        }

        if (block_size != 0x2000)
        {
            fprintf(stderr, "expected block size of 0x2000\n");
            exit(EXIT_FAILURE);
        }

        {
            /* assume DSP */
            uint32_t c_samples_per_block = block_size / 8 * 14;
            if (c_samples_per_block != samples_per_block)
            {
                fprintf(stderr,
                    "Computed samples per block %"PRIu32" != value in header\n",
                    c_samples_per_block);
                exit(EXIT_FAILURE);
            }
        }

        {
            uint32_t c_total_blocks =
                (sample_count+samples_per_block-1) / samples_per_block;
            if (c_total_blocks != total_blocks)
            {
                fprintf(stderr,
                        "Computed block count %"PRIu32" != value in header\n",
                        c_total_blocks);
                exit(EXIT_FAILURE);
            }
        }

        {
            uint32_t c_last_block_samples = sample_count % samples_per_block;
            if (c_last_block_samples != last_block_samples)
            {
                fprintf(stderr,
                        "Computed last block samples %"PRIu32" != value in "
                        "header\n", c_last_block_samples);
                exit(EXIT_FAILURE);
            }
        }

        {
            /* assume DSP */
            uint32_t c_last_block_used_bytes =
                (samples_to_nibbles(last_block_samples)+1)/2;

            if (c_last_block_used_bytes != last_block_used_bytes)
            {
                fprintf(stderr,
                        "Computed last block used bytes %"PRIu32" != value in "
                        "header\n", c_last_block_used_bytes);
                exit(EXIT_FAILURE);
            }
        }

        {
            uint32_t c_last_block_size =
                (last_block_used_bytes + 0x1f) / 0x20 * 0x20;

            if (c_last_block_size != last_block_size)
            {
                fprintf(stderr,
                        "Computed last block size %"PRIu32" != value in "
                        "header\n", c_last_block_size);
                exit(EXIT_FAILURE);
            }
        }

        {
            /* normal blocks */
            uint32_t c_data_bytes = (total_blocks-1) * block_size;
            /* last block */
            c_data_bytes += last_block_size;
            /* for all channels */
            c_data_bytes *= channel_count;
            /* plus header */
            c_data_bytes += 8 + get_32_be_seek(data_offset, infile);
            /* plus alignment */
            c_data_bytes += 0x1f;
            c_data_bytes /= 0x20;
            c_data_bytes *= 0x20;

            if (c_data_bytes != data_size + 8)
            {
                fprintf(stderr,
                        "Computed DATA size 0x%"PRIx32" != DATA chunk size\n",
                        c_data_bytes);
                exit(EXIT_FAILURE);
            }
        }

        if (samples_per_adpc_entry != 0x3800)
        {
            if (samples_per_adpc_entry != 0x400)
            {
                fprintf(stderr,
                        "Expected 0x3800 or 0x400 in "
                        "\"Samples per ADPC entry\" field\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                options |= OPTION_ALTERNATE_ADPC_COUNT;
                head_adpc_blocks = (block_size * (total_blocks-1) +
                    last_block_size) / samples_per_adpc_entry + 1;
            }
        }
        else
        {
            head_adpc_blocks = sample_count / samples_per_adpc_entry + 1;
        }

        if (bytes_per_adpc_entry != 4)
        {
            fprintf(stderr, "Expected \"Bytes per ADPC entry\" to be 4\n");
            exit(EXIT_FAILURE);
        }

        {
            uint32_t c_adpc_size =
                ((head_adpc_blocks*bytes_per_adpc_entry*channel_count + 8) +
                 0x1f)/0x20*0x20;
            if (c_adpc_size != adpc_size + 8)
            {
                fprintf(stderr, "Computed ADPC size 0x%"PRIx32" != ADPC chunk "
                        "size\n", c_adpc_size);
                exit(EXIT_FAILURE);
            }
        }


        /* second chunk (don't know what this means) */
        if (get_32_be_seek(second_chunk_offset + 0, infile) == 0x01000000)
        {
            expect_32(0x01000000, second_chunk_offset + 4, "??", infile);
            expect_32(0x58, second_chunk_offset + 8, "??", infile);
            if (channel_count == 1)
            {
                expect_32(0x01000000, second_chunk_offset + 0xc, "??", infile);
            }
            else
            {
                expect_32(0x02000100, second_chunk_offset + 0xc, "??", infile);
            }
        }
        else if (get_32_be_seek(second_chunk_offset + 0, infile) == 0x01010000)
        {
            expect_32(0x01010000, second_chunk_offset + 4, "??", infile);
            expect_32(0x58, second_chunk_offset + 8, "??", infile);
            expect_32(0x7f400000, second_chunk_offset + 0xc, "??", infile);
            expect_32(0, second_chunk_offset + 0x10, "??", infile);
            if (channel_count == 1)
            {
                expect_32(0x01000000, second_chunk_offset + 0x14, "??", infile);
            }
            else
            {
                expect_32(0x02000100, second_chunk_offset + 0x14, "??", infile);
            }

            options |= OPTION_SECOND_CHUNK_EXTRA;
        }

        /* third chunk (channel info) */
        expect_8(channel_count, third_chunk_offset + 0,
                "third chunk channel count", infile);
        expect_8(0, third_chunk_offset + 1, "third chunk", infile);
        expect_16(0, third_chunk_offset + 2, "third chunk padding", infile);
        uint8_t dsp_header[0x60] = {0};

        if (!examine_mode)
        {
            /* build common DSP header (assume DSP ADPCM coding throughout) */
            write_32_be(sample_count, dsp_header + 0);
            write_32_be(samples_to_nibbles(sample_count), dsp_header + 4);
            write_32_be(sample_rate, dsp_header + 8);
            write_16_be((loop_flag != 0), dsp_header + 0xc);
            write_16_be(0, dsp_header + 0xe); /* format = 0 */
            write_32_be(samples_to_nibbles(loop_start), dsp_header + 0x10);
            write_32_be(samples_to_nibbles(sample_count)-1, dsp_header + 0x14);
            write_32_be(0, dsp_header + 0x18); /* current address = 0 */
        }

        for (int i = 0; i < dsp_count; i++)
        {
            expect_32(0x01000000, third_chunk_offset + 4 + i*8, "dummy offset?",
                    infile);

            uint32_t channel_info_offset_offset = head_offset + get_32_be_seek(
                    third_chunk_offset + 4 + i * 8 + 4, infile);
            expect_32(0x01000000, channel_info_offset_offset, "dummy offset?",
                    infile);
            uint32_t channel_info_offset = head_offset +
                get_32_be_seek(channel_info_offset_offset + 4, infile);

            if (!examine_mode)
            {
                /* per-channel DSP header stuff */
                get_bytes_seek(channel_info_offset + 0, infile,
                        dsp_header + 0x1c, 0x30);

                CHECK_FILE(fwrite(dsp_header, 1, 0x60, outfiles[i])!=0x60,
                        outfiles[i], "fwrite");
            }
        }
    } /* end scan HEAD */

    /* make note of detected oddities */
    if (options != 0)
    {
        fprintf(stderr, "\n*** Odd format! To rebuild, use: ");
        if (options & OPTION_SECOND_CHUNK_EXTRA)
            fprintf(stderr, "--second-chunk-extra ");
        if (options & OPTION_ALTERNATE_ADPC_COUNT)
            fprintf(stderr, "--alternate-adpc-count ");
        fprintf(stderr, "***\n\n");
    }

    /* TODO: check that ADPC works like we think */

    /* Write out data */
    if (!examine_mode)
    {
        uint32_t data_body = data_offset + get_32_be_seek(data_offset, infile);
        for (int block = 0; block < total_blocks; block++)
        {
            if (block == total_blocks-1 && last_block_size) break;

            for (int i = 0; i < dsp_count; i++)
            {
                dump(infile, outfiles[i], data_body +
                        (block * dsp_count + i) * block_size, block_size);
            }
        }
        if (last_block_size)
        {
            for (int i = 0; i < dsp_count; i++)
            {
                dump(infile, outfiles[i], data_body +
                        (total_blocks-1) * dsp_count * block_size +
                        last_block_size * i, last_block_size);
            }
        }
    } /* end write out data */

    /* close files */
    if (!examine_mode)
    {
        for (int i = 0; i < dsp_count; i++)
        {
            CHECK_ERRNO(fclose(outfiles[i]) != 0, "fclose");
            outfiles[i] = NULL;
        }
    }
    CHECK_ERRNO(fclose(infile) != 0, "fclose");
    infile = NULL;

    fprintf(stderr, "Done!\n");
}

void build(const char *brstm_name, const char *dsp_names[],
        int dsp_count, enum odd_options options)
{
    FILE *outfile = NULL;
    FILE *infiles[MAX_CHANNELS];

    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        infiles[i] = NULL;
    }

    /* announce intentions */
    fprintf(stderr, "Building %s from:\n",brstm_name);
    for (int i = 0; i < dsp_count; i++)
    {
        fprintf(stderr, "  channel %d: %s\n", i, dsp_names[i]);
    }

    fprintf(stderr,"\n");

    /* open input files */
    for (int i = 0; i < dsp_count; i++)
    {
        infiles[i] = fopen(dsp_names[i], "rb");
        CHECK_ERRNO(infiles[i] == NULL, "fopen of input file");
    }

    /* open output file */
    outfile = fopen(brstm_name, "wb");
    CHECK_ERRNO(outfile == NULL, "fopen of output file");

    /* check that the DSPs agree */
    uint8_t dsp_header0[0x60] = {0};

    get_bytes_seek(0, infiles[0], dsp_header0, 0x1c);
    for (int i = 1; i < dsp_count; i++)
    {
        uint8_t dsp_header1[0x1c];
        get_bytes_seek(0, infiles[i], dsp_header1, 0x1c);
        if (memcmp(dsp_header0, dsp_header1, 0x1c))
        {
            fprintf(stderr, "DSP headers do not agree\n");
            exit(EXIT_FAILURE);
        }
    }

    /* read important elements of the header */
    uint32_t nibble_count = read_32_be(dsp_header0 + 4);
    const uint32_t sample_rate = read_32_be(dsp_header0 + 8);
    const uint16_t loop_flag = read_16_be(dsp_header0 + 0xc);
    if (read_16_be(dsp_header0 + 0xe) != 0)
    {
        fprintf(stderr, "source file is not DSP ADPCM\n");
        exit(EXIT_FAILURE);
    }
    const uint32_t loop_nibble = read_32_be(dsp_header0 + 0x10);

    /* truncate to loop end (note that standard DSP loop end is the
     * last nibble played, not one after) */
    if (loop_flag && read_32_be(dsp_header0 + 0x14)+1 < nibble_count)
        nibble_count = read_32_be(dsp_header0 + 0x14)+1;
    const uint32_t sample_count = nibbles_to_samples(nibble_count);

    /* now we can start building the file */
    uint32_t current_offset = 0;
    
    /* begin RSTM header */
    put_bytes_seek(current_offset, outfile, RSTM_sig, 4);
    put_32_be(0xFEFF0100, outfile);
    current_offset += 8;
    /* reserve space for file size */
    const uint32_t file_size_offset = current_offset;
    current_offset += 4;
    /* header size and ?? */
    put_16_be_seek(0x40, current_offset, outfile);
    put_16_be(0x2, outfile);
    current_offset += 4;
    /* reserve space for chunk info */
    const uint32_t head_chunk_offset_offset = current_offset;
    const uint32_t head_chunk_size_offset = current_offset + 4;
    current_offset += 8;
    const uint32_t adpc_chunk_offset_offset = current_offset;
    const uint32_t adpc_chunk_size_offset = current_offset + 4;
    current_offset += 8;
    const uint32_t data_chunk_offset_offset = current_offset;
    const uint32_t data_chunk_size_offset = current_offset + 4;
    current_offset += 8;

    /* pad */
    current_offset = pad(current_offset, 0x20, outfile);

    /* begin HEAD chunk header */
    const uint32_t head_chunk_offset = current_offset;
    put_32_be_seek(head_chunk_offset, head_chunk_offset_offset, outfile);
    put_bytes_seek(current_offset, outfile, head_name, 4);
    current_offset += 8;

    /* HEAD body */
    const uint32_t head_offset = current_offset;
    const uint32_t block_size = 0x2000;
    uint32_t last_block_size;
    uint32_t last_block_used_bytes;
    uint32_t block_count;
    uint32_t head_data_offset_offset;
    uint32_t samples_per_adpc_entry;
    uint32_t bytes_per_adpc_entry;
    {
        /* chunk list */
        put_32_be_seek(0x01000000, current_offset, outfile);
        const uint32_t first_chunk_offset_offset = current_offset + 4;
        current_offset += 8;
        put_32_be_seek(0x01000000, current_offset, outfile);
        const uint32_t second_chunk_offset_offset = current_offset + 4;
        current_offset += 8;
        put_32_be_seek(0x01000000, current_offset, outfile);
        const uint32_t third_chunk_offset_offset = current_offset + 4;
        current_offset += 8;

        /* first chunk */
        put_32_be_seek(current_offset-head_offset, first_chunk_offset_offset,
                outfile);
        put_byte_seek(2, current_offset, outfile);  /* DSP ADPCM */
        put_byte((loop_flag != 0),  outfile);
        put_byte(dsp_count, outfile);
        put_byte(0, outfile);   /* padding */
        put_16_be(sample_rate, outfile);
        put_16_be(0, outfile);  /* padding */
        put_32_be(nibbles_to_samples(loop_nibble), outfile);    /* loop start */
        put_32_be(sample_count, outfile);
        current_offset += 0x10;

        head_data_offset_offset = current_offset;
        current_offset += 4;

        /* DSP-centric */
        const uint32_t samples_per_block = nibbles_to_samples(block_size*2);
        if ( nibbles_to_samples(loop_nibble) % samples_per_block != 0 )
        {
            fprintf(stderr, "Warning!\n"
                    "   Loop start sample %" PRIu32 " is not on "
                    "a block boundary.\n"
                    "   The brstm may not loop properly "
                    "(blocks are %" PRIu32 " samples).\n"
                    "   This can be solved by adding %" PRIu32 " samples of\n"
                    "   silence to the beginning of the track.\n\n",
                    nibbles_to_samples(loop_nibble), samples_per_block,
                    samples_per_block -
                    (nibbles_to_samples(loop_nibble) % samples_per_block)
                    );
        }
        block_count = (sample_count + samples_per_block-1) / samples_per_block;
        put_32_be_seek(block_count, current_offset, outfile);
        put_32_be(block_size, outfile);
        put_32_be(samples_per_block, outfile);
        const uint32_t last_block_samples = sample_count % samples_per_block;
        last_block_used_bytes = (samples_to_nibbles(last_block_samples)+1)/2;
        last_block_size = (last_block_used_bytes + 0x1f) / 0x20 * 0x20;
        put_32_be(last_block_used_bytes, outfile);
        put_32_be(last_block_samples, outfile);
        put_32_be(last_block_size, outfile);

        if (options & OPTION_ALTERNATE_ADPC_COUNT)
            samples_per_adpc_entry = 0x400;
        else
            samples_per_adpc_entry = samples_per_block;
        put_32_be(samples_per_adpc_entry, outfile);
        bytes_per_adpc_entry = 4;
        put_32_be(bytes_per_adpc_entry, outfile);
        current_offset += 0x20;

        /* second chunk (don't know what this means) */
        put_32_be_seek(current_offset-head_offset, second_chunk_offset_offset,
                outfile);
        if (options & OPTION_SECOND_CHUNK_EXTRA)
        {
            put_32_be_seek(0x01010000, current_offset, outfile);
            put_32_be(0x01010000, outfile);
            put_32_be(0x58, outfile);
            put_32_be(0x7f400000, outfile);
            put_32_be(0, outfile);
            if (dsp_count == 1)
                put_32_be(0x01000000, outfile);
            else
                put_32_be(0x02000100, outfile);
            current_offset += 0x18;
        }
        else
        {
            put_32_be_seek(0x01000000, current_offset, outfile);
            put_32_be(0x01000000, outfile);
            put_32_be(0x58, outfile);
            if (dsp_count == 1)
                put_32_be(0x01000000, outfile);
            else
                put_32_be(0x02000100, outfile);
            current_offset += 0x10;
        }

        /* third chunk */
        put_32_be_seek(current_offset-head_offset, third_chunk_offset_offset,
                outfile);
        put_byte_seek(dsp_count, current_offset, outfile);
        put_bytes(outfile, (const uint8_t *)"\0\0", 3);   /* padding */
        current_offset += 4;
        uint32_t channel_info_offset_3x[MAX_CHANNELS];
        for (int i = 0; i < dsp_count; i++)
        {
            put_32_be_seek(0x01000000, current_offset, outfile);
            current_offset+=4;
            channel_info_offset_3x[i] = current_offset;
            current_offset+=4;
        }
        uint32_t channel_info_offset_2x[MAX_CHANNELS];
        for (int i = 0; i < dsp_count; i++)
        {
            put_32_be_seek(current_offset-head_offset,
                    channel_info_offset_3x[i], outfile);
            put_32_be_seek(0x01000000, current_offset, outfile);
            current_offset+=4;
            channel_info_offset_2x[i] = current_offset;
            current_offset+=4;

            put_32_be_seek(current_offset-head_offset,
                    channel_info_offset_2x[i], outfile);
            uint8_t dsp_header_channel_info[0x30];
            get_bytes_seek(0x1c, infiles[i], dsp_header_channel_info, 0x30);
            put_bytes(outfile, dsp_header_channel_info, 0x30);
            current_offset += 0x30;
        }

        /* pad */
        current_offset = pad(current_offset, 0x20, outfile);

        /* done with HEAD chunk, store size */
        put_32_be_seek(current_offset-head_chunk_offset,head_chunk_size_offset,
                outfile);
        put_32_be_seek(current_offset-head_chunk_offset,head_chunk_offset+4,
                outfile);
    } /* end write HEAD chunk */

    /* begin ADPC chunk header */
    const uint32_t adpc_chunk_offset = current_offset;
    put_32_be_seek(adpc_chunk_offset, adpc_chunk_offset_offset, outfile);
    put_bytes_seek(current_offset, outfile, adpc_name, 4);
    current_offset += 8;

    /* ADPC body */
    {
        uint32_t adpc_blocks;
        if (options & OPTION_ALTERNATE_ADPC_COUNT)
        {
            adpc_blocks = (block_size * (block_count-1) + last_block_size) /
                0x400 + 1;
        }
        else
        {
            adpc_blocks = sample_count / samples_per_adpc_entry + 1;
        }

        /* TODO actually fill in ADPC values */
        current_offset +=
            adpc_blocks * bytes_per_adpc_entry * dsp_count;

        /* pad */
        current_offset = pad(current_offset, 0x20, outfile);

        /* done with ADPC chunk, store size */
        put_32_be_seek(current_offset-adpc_chunk_offset,adpc_chunk_size_offset,
                outfile);
        put_32_be_seek(current_offset-adpc_chunk_offset,adpc_chunk_offset+4,
                outfile);
    } /* end write ADPC chunk */

    /* begin DATA chunk header */
    const uint32_t data_chunk_offset = current_offset;
    put_32_be_seek(data_chunk_offset, data_chunk_offset_offset, outfile);
    put_bytes_seek(current_offset, outfile, data_name, 4);
    current_offset += 8;

    /* DATA body */
    {
        uint32_t infile_offset = 0x60;
        CHECK_ERRNO(fseek(outfile, current_offset, SEEK_SET) != 0, "fseek");

        put_32_be(0x18, outfile);
        current_offset += 0x18;
        put_32_be_seek(current_offset, head_data_offset_offset, outfile);
        CHECK_ERRNO(fseek(outfile, current_offset, SEEK_SET) != 0, "fseek");

        for (uint32_t i = 0; i < block_count; i++)
        {
            if (i == block_count-1 && last_block_size) break;

            for (int channel = 0; channel < dsp_count; channel++)
            {
                dump(infiles[channel], outfile, infile_offset, block_size);
                current_offset += block_size;
            }

            infile_offset += block_size;
        }
        if (last_block_size)
        {
            for (int channel = 0; channel < dsp_count; channel++)
            {
                CHECK_ERRNO(fseek(outfile, current_offset, SEEK_SET) != 0,
                        "fseek");
                dump(infiles[channel], outfile, infile_offset,
                        last_block_used_bytes);

                /* pad */
                for (uint32_t i = last_block_used_bytes; i < last_block_size;
                        i++)
                {
                    put_byte(0, outfile);
                }
                current_offset += last_block_size;
            }
        }

        /* pad */
        current_offset = pad(current_offset, 0x20, outfile);

        /* done with DATA chunk, store size */
        put_32_be_seek(current_offset-data_chunk_offset,data_chunk_size_offset,
                outfile);
        put_32_be_seek(current_offset-data_chunk_offset,data_chunk_offset+4,
                outfile);
    } /* end write DATA chunk */

    /* done with file */
    put_32_be_seek(current_offset, file_size_offset, outfile);

    /* close files */
    for (int i = 0; i < dsp_count; i++)
    {
        CHECK_ERRNO(fclose(infiles[i]) != 0, "fclose");
        infiles[i] = NULL;
    }
    CHECK_ERRNO(fclose(outfile) != 0, "fclose");
    outfile = NULL;

    fprintf(stderr, "Done!\n");
}
