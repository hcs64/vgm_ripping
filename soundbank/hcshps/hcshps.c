#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

#include "error_stuff.h"
#include "util.h"

/* hcshps - .hps (HALPST) building and extraction tool */
/* by hcs (http://here.is/halleyscomet) */

#define VERSION "0.0"
#define MAX_CHANNELS 2

enum prog_mode
{
    MODE_INVALID,
    MODE_BUILD,
    MODE_EXTRACT,
    MODE_EXAMINE,
};

struct DSP_decode_state
{
    int16_t coef[16];
    uint16_t ps;
    int16_t hist1;
    int16_t hist2;
};
struct channel_info
{
    //uint32_t max_block_size;
    //uint32_t loop_start_nibble;
    uint32_t last_nibble;
    //uint32_t current_nibble;
    struct DSP_decode_state state;
};

static void extract(const char *brstm_name, const char *dsp_names [],
        int dsp_count);
static void build(const char *brstm_name, const char *dsp_names [],
        int dsp_count);
static uint32_t samples_to_nibbles(uint32_t samples);
static uint32_t nibbles_to_samples(uint32_t nibbles);
void decode_dsp_nowhere(struct DSP_decode_state *state, long start_offset, uint32_t start_nibble, uint32_t end_nibble, FILE *infile);

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
            "hcs's .hps utility\n"
            "Version " VERSION " (built " __DATE__ ")\n\n"
            "Build .hps files from mono .dsp (or extract)\n"
            "examine usage:\n"
            "    %s --examine source.hps\n"
            "build usage:\n"
            "    %s --build dest.hps source.dsp [sourceR.dsp]\n"
            "extract usage:\n"
            "    %s --extract source.hps dest.dsp [destR.dsp]\n"
            "\n",
            bin_name, bin_name, bin_name);

    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    const char *hps_name = NULL;
    const char *dsp_names[MAX_CHANNELS];
    enum prog_mode mode = MODE_INVALID;
    int dsp_count = 0;

    /* for usage() */
    bin_name = argv[0];

    /* clear dsp_names */
    for (int c = 0; c < MAX_CHANNELS; c++) dsp_names[c] = NULL;

    /* process arguments */
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp("--extract", argv[i]))
        {
            if (mode != MODE_INVALID) usage();
            if (i >= argc-1) usage();

            mode = MODE_EXTRACT;
            hps_name = argv[++i];
        }
        else if (!strcmp("--build", argv[i]))
        {
            if (mode != MODE_INVALID) usage();
            if (i >= argc-1) usage();

            mode = MODE_BUILD;
            hps_name = argv[++i];
        }
        else if (!strcmp("--examine", argv[i]))
        {
            if (mode != MODE_INVALID) usage();
            if (i >= argc-1) usage();

            mode = MODE_EXAMINE;
            hps_name = argv[++i];
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
    if (mode == MODE_EXAMINE && dsp_count != 0) usage();
    if (mode != MODE_EXAMINE && dsp_count == 0) usage();

    switch (mode)
    {
        case MODE_BUILD:
            build(hps_name, dsp_names, dsp_count);
            break;
        case MODE_EXAMINE:
        case MODE_EXTRACT:
            extract(hps_name, dsp_names, dsp_count);
            break;
        case MODE_INVALID:
            usage();
            break;
    }

    return 0;
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

static const uint8_t HALPST_sig[8] = {' ','H','A','L','P','S','T','\0'};

static void extract(const char *brstm_name, const char *dsp_names [],
        int dsp_count)
{
    FILE *infile = NULL;
    long infile_size;
    FILE *outfiles[MAX_CHANNELS];

    int examine_mode = (dsp_count == 0);

    for (int c = 0; c < MAX_CHANNELS; c++)
    {
        outfiles[c] = NULL;
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
        for (int c = 0; c < dsp_count; c++)
        {
            outfiles[c] = fopen(dsp_names[c], "wb");
            CHECK_ERRNO(outfiles[c] == NULL, "fopen of output file");
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
        for (int c = 0; c < dsp_count; c++)
        {
            fprintf(stderr, "  channel %d: %s\n", c, dsp_names[c]);
        }
    }

    fprintf(stderr,"\n");

    /* read HALPST header */
    {
        uint8_t buf[8];

        /* check signature */
        get_bytes_seek(0, infile, buf, 8);

        if (memcmp(HALPST_sig, buf, 8))
        {
            fprintf(stderr,"no HALPST header\n");
            exit(EXIT_FAILURE);
        }
    }

    /* load and check header */
    uint32_t sample_rate;
    uint32_t channel_count;
    struct channel_info info[MAX_CHANNELS];

    {
        sample_rate = get_32_be_seek(0x08, infile);
        channel_count = get_32_be_seek(0x0C, infile);
        
        for (int c = 0; c < channel_count; c++)
        {
            long info_offset = 0x10 + c * 0x38;
            expect_32(0x10000, info_offset + 0x00, "max block size", infile);
            expect_32(2, info_offset + 0x04, "loop start nibble", infile);
            info[c].last_nibble = get_32_be_seek(info_offset + 0x08, infile);
            expect_32(2, info_offset + 0x0C, "current nibble", infile);
            for (int i = 0; i < 16; i++)
            {
                info[c].state.coef[i] = get_16_be_seek(info_offset + 0x10 + i*2,
                    infile);
            }
            expect_16(0, info_offset + 0x30, "gain", infile);
            info[c].state.ps = get_16_be_seek(info_offset + 0x32, infile);
            info[c].state.hist1 = get_16_be_seek(info_offset + 0x34, infile);
            info[c].state.hist2 = get_16_be_seek(info_offset + 0x36, infile);

            /* check all channels for agreement on common stuff */
            if ( c > 0 )
            {
                CHECK_ERROR(info[c].last_nibble != info[0].last_nibble,
                    "nibble count mismatch between channels");
            }
        }

        /* check that remaining header space is zeroes */
        for (int c = channel_count; c < MAX_CHANNELS; c++)
        {
            long info_offset = 0x10 + c * 0x38;
            for (int i = 0; i < 0x38; i += 4)
            {
                expect_32(0, info_offset + i, "zero padding in header", infile);
            }
        }

        /* TODO: print stream info */

        /* check stream info */
        if (!examine_mode && channel_count != dsp_count)
        {
            fprintf(stderr,
                    "stream has %d channels, but %d DSPs were specified\n",
                    channel_count, dsp_count);
            exit(EXIT_FAILURE);
        }
    }

    /* output initial info in DSP header */
    if (!examine_mode)
    {
        for (int c = 0; c < channel_count; c++)
        {
            /* sample count */
            uint32_t sample_count = nibbles_to_samples(info[c].last_nibble)+1;
            put_32_be_seek(sample_count, 0x00, outfiles[c]);
            /* nibble count */
            put_32_be_seek(info[c].last_nibble+1, 0x04, outfiles[c]);
            /* sample rate */
            put_32_be_seek(sample_rate, 0x08, outfiles[c]);
            /* loop flag (assume nonlooped for now) */
            put_16_be_seek(0, 0x0c, outfiles[c]);
            /* format (0=DSP ADPCM) */
            put_16_be_seek(0, 0x0e, outfiles[c]);
            /* loop start offset (zero for now) */
            put_32_be_seek(0, 0x10, outfiles[c]);
            /* loop end offset (zero for now) */
            put_32_be_seek(0, 0x14, outfiles[c]);
            /* current offset (2=beginning of stream) */
            put_32_be_seek(2, 0x18, outfiles[c]);
            for (int i = 0; i < 16; i++)
            {
                put_16_be_seek(info[c].state.coef[i], 0x1c + i*2, outfiles[c]);
            }
            /* gain (0 for ADPCM) */
            put_16_be_seek(0, 0x3c, outfiles[c]);
            /* initial P/S */
            put_16_be_seek(info[c].state.ps, 0x3e, outfiles[c]);
            /* initial hist1 */
            put_16_be_seek(info[c].state.hist1, 0x40, outfiles[c]);
            /* initial hist2 */
            put_16_be_seek(info[c].state.hist2, 0x42, outfiles[c]);
            /* loop P/S (assume 0) */
            put_16_be_seek(0, 0x44, outfiles[c]);
            /* loop hist1 (assume 0) */
            put_16_be_seek(0, 0x46, outfiles[c]);
            /* loop hist2 (assume 0) */
            put_16_be_seek(0, 0x48, outfiles[c]);

            /* fill rest with zeroes */
            for (int i = 0x4a; i < 0x60; i++)
                put_byte(0, outfiles[c]);
        }
    }

    /* process stream block-by-block */
    int loop_flag = 0;
    {
        long last_block_offset = 0;
        long block_offset = 0x80;
        long nibbles_left = info[0].last_nibble+1;

        for (;;)
        {
            const uint32_t block_size_total =
                get_32_be_seek(block_offset + 0x00, infile);
            CHECK_ERROR(block_size_total % channel_count != 0,
                    "total block size not multiple of channel count");
            const uint32_t block_size = block_size_total / channel_count;
            const uint32_t block_nibbles =
                get_32_be_seek(block_offset + 0x04, infile) + 1;

            int loop_pass = 0;

            /* looped, one additional pass to check state at loop point */
            if (last_block_offset >= block_offset) loop_pass = 1;

            if (!loop_pass)
            {
                if (nibbles_left > block_nibbles)
                {
                    /* there are more blocks, so this should not be a partial
                       block, and every byte should be accounted for */
                    CHECK_ERROR(block_nibbles != block_size*2,
                        "block_nibbles != block_size*2");
                }
                else
                {
                    /* this is a partial block, should have the remaining
                       nibbles */

                    CHECK_ERROR(nibbles_left != block_nibbles,
                        "partial block nibble count not equal to remainder");
                    CHECK_ERROR(block_nibbles > block_size*2,
                        "block_nibbles > block_size*2");
                }
                nibbles_left -= block_nibbles;
            }
            else
            {
                CHECK_ERROR(nibbles_left != 0, "not all nibbles consumed before loop");
            }


            /* update state, check for consistency */
            for (int c = 0; c < channel_count; c++)
            {
                const long block_channel_offset = block_offset + 0x0C + 8*c;
                const long data_offset = block_offset + 0x20 + block_size*c;

                const uint16_t block_init_ps =
                    get_16_be_seek(block_channel_offset + 0x00, infile);
                const int16_t block_init_hist1 =
                    get_16_be_seek(block_channel_offset + 0x02, infile);
                const int16_t block_init_hist2 =
                    get_16_be_seek(block_channel_offset + 0x04, infile);

                expect_16(0, block_channel_offset + 0x06,
                        "block chan info padding?", infile);

                info[c].state.ps = get_byte_seek(data_offset, infile);
                CHECK_ERROR(info[c].state.ps != block_init_ps,
                        "block header init P/S != first P/S in block");
                CHECK_ERROR(info[c].state.hist1 != block_init_hist1,
                        "history1 mismatch");
                CHECK_ERROR(info[c].state.hist2 != block_init_hist2,
                        "history2 mismatch");
            }

            /* check padding for missing channel */
            for (int c = channel_count; c < MAX_CHANNELS; c++)
            {
                const long block_channel_offset = 0x80 + 0x0C + 8*c;
                for (int i = 0; i < 8; i += 4)
                    expect_32(0, block_channel_offset+i,
                            "zero padding in block header", infile);
            }

            /* padding at end of header */
            expect_32(0, block_offset + 0x1C,
                    "zero padding in block header", infile);

            /* decode to advance state */
            for (int c = 0; c < channel_count; c++)
            {
                const long data_offset = block_offset + 0x20 + block_size*c;
                const uint32_t block_nibbles = block_size * 2;
                decode_dsp_nowhere(&info[c].state, data_offset+1, 2, block_nibbles-1, infile);
            }

            /* advance to next block */
            last_block_offset = block_offset;
            block_offset = get_32_be_seek(block_offset + 0x08, infile);

            /* end, loop */
            if (loop_pass) break;
            /* end, no loop */
            if (block_offset == UINT32_C(0xFFFFFFFF)) break;
        }

        if (block_offset == UINT32_C(0xFFFFFFFF)) loop_flag = 0;
        else loop_flag = 1;
    }

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
        int dsp_count)
{
}
#if 0
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
#endif

void decode_dsp_nowhere(struct DSP_decode_state *state, long start_offset, uint32_t start_nibble, uint32_t end_nibble, FILE *infile)
{
    uint32_t current_nibble = start_nibble;
    long offset = start_offset;

    int scale = state->ps & 0xf;
    int coef_index = (state->ps >> 4) & 0xf;
    int32_t coef1 = state->coef[coef_index*2+0];
    int32_t coef2 = state->coef[coef_index*2+1];

    do 
    {
        //if (state->hist1 == -478 && state->hist2 == -2938) printf("offset=%lx current_nibble = %"PRIx32"\n",offset,current_nibble);
        if (0 == current_nibble % 16)
        {
            state->ps = get_byte_seek(offset, infile);

            scale = state->ps & 0xf;
            coef_index = (state->ps >> 4) & 0xf;
            coef1 = state->coef[coef_index*2+0];
            coef2 = state->coef[coef_index*2+1];

            offset ++;
            current_nibble += 2;
        }
        else
        {
            uint8_t byte = get_byte_seek(offset, infile);
            int32_t delta;
            if (current_nibble % 2 == 1) delta = byte & 0xf;
            else delta = (byte >> 4) & 0xf;

            if (delta >= 8) delta -= 16;

            int32_t sample = (
                        (delta << (scale + 11)) + 
                        (coef1 * state->hist1 + coef2 * state->hist2) +
                        1024
                             ) >> 11;
            int16_t sample16 = sample;
            if (sample > 0x7FFF) sample16 = 0x7FFF;
            else if (sample < -0x8000) sample16 = -0x8000;

            state->hist2 = state->hist1;
            state->hist1 = sample16;

            if (current_nibble % 2 == 1) offset ++;
            current_nibble ++;
        }
    }
    while (end_nibble >= current_nibble);
}
