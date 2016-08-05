#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "error_stuff.h"
#include "util.h"

void process_wave(
    FILE *headerfile,
    FILE *datafile,
    long riff_offset
    );

int main(int argc, char **argv)
{
    fprintf(stderr, "build GENH from decompressed Conduit dumps\n");

    /* input file name 1 (index) */
    const char *indexfile_name = NULL;
    /* input file name 2 (headers) */
    const char *headerfile_name = NULL;
    /* input file name 3 (WAVS data) */
    const char *datafile00_name = NULL;
    /* input file name 4 (mstr data) */
    const char *datafilemstr_name = NULL;
    /* input file name 5 (WAVE data) */
    const char *datafile01_name = NULL;
    CHECK_ERROR(argc < 4 || argc > 6, "usage: exe dump01.bin dump00.bin dumpUC00.bin [dumpUCmstr.bin [dumpUC01.bin]]");

    indexfile_name = argv[1];
    headerfile_name = argv[2];
    datafile00_name = argv[3];
    if (argc > 4)
        datafilemstr_name = argv[4];
    if (argc > 5)
        datafile01_name = argv[5];

    FILE *indexfile = fopen(indexfile_name, "rb");
    CHECK_ERRNO(NULL == indexfile, "input file 1 open failed");
    FILE *headerfile = fopen(headerfile_name, "rb");
    CHECK_ERRNO(NULL == headerfile, "input file 2 open failed");
    FILE *datafile00 = fopen(datafile00_name, "rb");
    CHECK_ERRNO(NULL == datafile00, "input file 3 open failed");
    FILE *datafilemstr = NULL;
    if (datafilemstr_name)
    {
        datafilemstr = fopen(datafilemstr_name, "rb");
        CHECK_ERRNO(NULL == datafilemstr, "input file 4 open failed");
    }
    FILE *datafile01 = NULL;
    if (datafile01_name)
    {
        datafile01 = fopen(datafile01_name, "rb");
        CHECK_ERRNO(NULL == datafile01, "input file 5 open failed");
    }

    /* process index */
    long offset;
    CHECK_ERRNO( -1 == fseek(indexfile,0,SEEK_END), "fseek" );
    long file_size = ftell(indexfile);
    CHECK_ERRNO( -1 == file_size, "ftell" );
    for (offset = 0; offset < file_size; )
    {
        uint32_t item_type = get_32_be_seek(offset, indexfile);
        offset += 4;
        switch (item_type)
        {
            case 0x80000113:    /* RIFF */
            {
                long riff_offset = get_32_be_seek(offset,indexfile);

                unsigned char riff_buf[12];

                get_bytes_seek(riff_offset, headerfile, riff_buf, 12);

                if (0 != memcmp(&riff_buf[0], "RIFF", 4))
                {
                    fprintf(stderr, "non-RIFF header at 0x%lx\n",
                            riff_offset);
                    CHECK_ERROR(1, "non-RIFF header, possible bad data00.bin?");
                }

                if (0 == memcmp(&riff_buf[8], "WAVS", 4))
                {
                    process_wave(headerfile,datafile00,riff_offset);
                }
                else if ( 0 == memcmp(&riff_buf[8], "WAVE", 4))
                {
                    process_wave(headerfile,datafile01,riff_offset);
                }
                else if ( 0 == memcmp(&riff_buf[8], "mstr", 4))
                {
                    process_wave(headerfile,datafilemstr,riff_offset);
                }
                else if ( 0 == memcmp(&riff_buf[8], "AMPC", 4))
                { /* ignore AMPC */ }
                else
                {
                    fprintf(stderr, "unexpected RIFF type at 0x%lx\n",
                            riff_offset);
                    CHECK_ERROR(1, "unexpected RIFF type, possible bad data00.bin?");
                }

                offset += 4;
                break;
            }
            case 0x8000011a:    /* ??? */
            case 0x80000118:    /* string table */
            case 0x80000117:    /* ??? */
            case 0x80000115:    /* ??? */
            case 0x8000010E:    /* ??? */
            case 0x8000010A:    /* ??? */
            case 0x80000007:    /* ??? */
            case 0x80000006:    /* ??? */
            case 0x80000005:    /* ??? */
            case 0x80000004:    /* ??? */
            case 0x80000001:    /* ??? */
                while (offset < file_size && 0 == (get_32_be_seek(offset,indexfile) & 0x80000000))
                    offset +=4;
                break;
            default:
                fprintf(stderr,"unknown type %08"PRIx32"\n",item_type);
                CHECK_ERROR(1, "unknown type");
                break;
        }
    }

}

void process_wave(
    FILE *headerfile,
    FILE *datafile,
    long riff_offset
    )
{
    long chunk_offset = riff_offset + 12;
    long riff_size = get_32_le_seek(riff_offset + 4, headerfile);
    unsigned char wave_buf[4];

    get_bytes_seek(riff_offset + 8, headerfile, wave_buf, 4);

    /* things we are harvesting */
    uint32_t sample_count;
    int sample_count_found = 0;
    uint32_t loop_start_sample;
    uint32_t loop_end_sample;
    int loop_found = 0;
    long name_offset = -1;
    uint32_t name_size;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t codec_id;
    uint16_t block_align;
    uint16_t bits_per_sample;
    int fmt_found = 0;
    long data_start_offset;
    long data_length;
    int data_info_found = 0;
    long gc_header_offset = -1;

    /* scan for info */
    while (chunk_offset < riff_offset + 8 + riff_size)
    {
        switch (get_32_be_seek(chunk_offset, headerfile))
        {
            case UINT32_C(0x67756964):  /* guid */
                break;
            case UINT32_C(0x6e616d65):  /* name */
                name_offset = chunk_offset + 8;
                name_size = get_32_le_seek(chunk_offset + 4, headerfile);
                break;
            case UINT32_C(0x666d7420):  /* fmt */
                /* check codec */
                switch ( get_16_le_seek(chunk_offset + 8, headerfile) )
                {
                    case 0x666:
                    case 0x1:
                        break;
                    default:
                        CHECK_ERROR(1, "only expecting codec id 0x666 or 0x1");
                        break;
                }
                codec_id = get_16_le_seek(chunk_offset + 8, headerfile);
                channels = get_16_le_seek(chunk_offset + 10, headerfile);
                sample_rate = get_32_le_seek(chunk_offset + 12, headerfile);
                block_align = get_16_le_seek(chunk_offset + 20, headerfile);
                bits_per_sample = get_16_le_seek(chunk_offset + 22, headerfile);
                fmt_found = 1;
                break;
            case UINT32_C(0x66616374):  /* fact */
                sample_count = get_32_le_seek(chunk_offset + 8, headerfile);
                sample_count_found = 1;
                break;
            case UINT32_C(0x6e676363):  /* ngcc */
                gc_header_offset = chunk_offset + 8;
                break;
            case UINT32_C(0x7374726d):  /* strm */
                data_start_offset = get_32_le_seek(chunk_offset + 8, headerfile);
                data_length = get_32_le_seek(chunk_offset + 12, headerfile);
                data_info_found = 1;
                break;
            case UINT32_C(0x77736d70):  /* wsmp */
                /* TODO: loop info? */
                break;
            default:
                fprintf(stderr, "unexpected chunk at 0x%lx\n", chunk_offset);
                CHECK_ERROR(1, "who knows?");
                break;
        }
        chunk_offset += 8 + get_32_le_seek(chunk_offset + 4, headerfile);
    }

    if ( fmt_found && data_info_found && 1 == codec_id )
    {
        CHECK_ERROR( sample_count_found,
            "didn't expect fact for uncompressed data");
        sample_count = data_length * 8 / channels / bits_per_sample;
        sample_count_found = 1;
    }

    CHECK_ERROR(
        !sample_count_found || -1 == name_offset || !fmt_found || !data_info_found || (codec_id == 0x666 && -1 == gc_header_offset),
        "missing vital info from RIFF");

    CHECK_ERROR( channels != 1 && channels != 2,
        "not mono or stereo");

    /* build GENH */
    FILE *outfile = NULL;
    {
        /* open file with stored file name */
        char namebuf[name_size + 1];
        char new_namebuf[name_size + 1 + 6];
        memset(namebuf,0,sizeof(namebuf));
        get_bytes_seek(name_offset, headerfile, (uint8_t*)namebuf, name_size);
        snprintf(new_namebuf, sizeof(new_namebuf), "%s.genh",namebuf);
        outfile = fopen(new_namebuf, "wb");
        CHECK_ERRNO(NULL == outfile, "fopen of output");

        {
            unsigned char riff_buf[12];

            get_bytes_seek(riff_offset, headerfile, riff_buf, 12);

            printf("%c%c%c%c data from %08lx size %08lx %s\n", riff_buf[8],riff_buf[9],riff_buf[10],riff_buf[11],data_start_offset,data_length,new_namebuf);
        }
    }

    unsigned char genh_head[0x38+0x60];
    memset(genh_head, 0, sizeof(genh_head));

    /* header magic */
    memcpy(&genh_head[0x00], "GENH", 4);

    /* channel count */
    write_32_le(channels, &genh_head[0x04]);

    /* coding type */
    switch (codec_id)
    {
        case 0x666:
            /* coding_NGC_DSP */
            write_32_le(12, &genh_head[0x18]);
            break;
        case 0x1:
            switch (bits_per_sample)
            {
                case 16:
                    /* coding_PCM16BE */
                    write_32_le(3, &genh_head[0x18]);
                    break;
                default:
                    CHECK_ERROR(1, "unknown bitdepth");
            }
    }

    /* start_offset */
    write_32_le(0x38 + 0x60, &genh_head[0x1c]);

    /* header_size */
    write_32_le(0x38, &genh_head[0x20]);

    /* interleave */
    switch (codec_id)
    {
        case 0x666:
            if (1 == channels)
            {
                write_32_le(0, &genh_head[0x08]);
            }
            else
            {
                // no interleave, but GENH does split data like this
                write_32_le(data_length/channels, &genh_head[0x08]);
            }
            break;
        case 0x1:
            write_32_le(block_align, &genh_head[0x08]);
            break;
    }

    /* sample rate */
    write_32_le(sample_rate, &genh_head[0x0c]);

    /* loop start TODO */
    write_32_le(~UINT32_C(0), &genh_head[0x10]);

    /* loop end (sample count) */
    write_32_le(sample_count, &genh_head[0x14]);

    /* dsp coefs */
    if ( 0x666 == codec_id )
    {
        write_32_le(0x38, &genh_head[0x24]);
        get_bytes_seek(gc_header_offset, headerfile, &genh_head[0x38], 0x30);
        if ( 2 == channels )
        {
            get_bytes_seek(gc_header_offset+0x30, headerfile, &genh_head[0x38+0x30], 0x30);
            write_32_le(0x38+0x30, &genh_head[0x28]);
        }
        /* normal type */
        write_32_le(0, &genh_head[0x2c]);
        if (1 == channels)
            /* no interleave */
            write_32_le(2, &genh_head[0x2c]);
    }

    put_bytes(outfile, genh_head, 0x38+0x60);
    dump(datafile, outfile, data_start_offset, data_length);

    CHECK_ERRNO(EOF == fclose(outfile), "fclose of output file");
}
