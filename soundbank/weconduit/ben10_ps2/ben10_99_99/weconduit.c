#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "error_stuff.h"
#include "util.h"

long decompress(
        FILE * const infile,
        FILE * const outfile,
        const long start_offset,
        const long bytes_to_decompress);

const long uncompressed_alignment = 0x800;

int main(int argc, char **argv)
{

    printf("decompressor for Conduit .gcs/.gcm\n");

    /* input file name */
    const char *infile_name = NULL;
    CHECK_ERROR(argc != 2, "needs one argument: file to work on");
    infile_name = argv[1];

    /* input file */
    FILE *infile = fopen(infile_name, "rb");
    CHECK_ERRNO(NULL == infile, "input file open failed");

    printf("\n");
    printf("Working on %s...\n", infile_name);

    /* check header */
    unsigned char header_buf[0x200];
    get_bytes_seek(0, infile, header_buf, 0x200);
    CHECK_ERROR( 0 != memcmp("BIGB", &header_buf[0x00], 4), "BIGB header chunk not found" );
    
    long data_start = read_32_le(&header_buf[0x04]) + 0x10;

    CHECK_ERROR( 0xA3 != read_32_le(&header_buf[0x08]), "bad signature at 0x08" );
    CHECK_ERROR( 1 != read_32_le(&header_buf[0x0c]), "bad signature at 0x0c" );
    CHECK_ERROR( 1 != header_buf[0x10], "bad signature at 0x10" );
    CHECK_ERROR( 0 != memcmp("\0JerryKaraganis\0", &header_buf[0x50], 16), "Jerry's signature not found" );

    printf("File Description: \"%s\"\n", &header_buf[0x11]);
    printf("Creation Arguments: \"%s\"\n", &header_buf[0x94]);

    printf("\n");

    {
        /* print segments */

        int uncompressed_segment_number = 0;
        printf("Segment List:\n");
        for (long offset = 0x60; offset < 0x94; offset += 4)
        {
            if (read_32_le(&header_buf[offset]) != 0)
            {
                printf("%02lx: %08"PRIx32": ", (unsigned long)offset, read_32_le(&header_buf[offset]));
                switch (offset) 
                {
                    case 0x84:
                        printf("segment 00 uncompressed size");
                        break;
                    case 0x88:
                        printf("segment 01 uncompressed size");
                        break;
                    case 0x8c:
                        printf("segment 00 compressed size");
                        break;
                    case 0x90:
                        printf("segment 01 compressed size");
                        break;
                    default:
                        printf("uncompressed segment %02x size",
                                uncompressed_segment_number ++);
                }
                printf("\n");
            }
        }
        printf("\n");
    }

    {
        /* handle compressed segments */

        /* two compressed segments always */
        const int segment_count = 2;
        const long segment_info_uncomp_offsets[2] = { 0x84, 0x88 };
        const long segment_info_comp_offsets[2] = { 0x8c, 0x90 };

        long data_offset = data_start;

        /* decompress each segment */
        for (int segment_number = 0; segment_number < segment_count; segment_number ++)
        {
            const long uncompressed_size =
                read_32_le(&header_buf[segment_info_uncomp_offsets[segment_number]]);
            const long compressed_size =
                read_32_le(&header_buf[segment_info_comp_offsets[segment_number]]);

            /* output file */
            char file_name[128];
            snprintf(file_name,sizeof(file_name),"dump%02d.bin", segment_number);
            FILE *outfile = fopen(file_name, "w+b");
            CHECK_ERRNO(NULL == outfile, file_name);

            /* begin! */
            printf("decompressing segment %02d\n", segment_number);
            printf("    (offset 0x%lx, 0x%lx bytes compressed, 0x%lx bytes uncompressed)\n",
                    data_offset, compressed_size, uncompressed_size);

            const long endoff = decompress(infile, outfile, data_offset, uncompressed_size);

            CHECK_ERRNO(EOF == fclose(outfile), "fclose of output file");
            outfile = NULL;

            printf("    done! (consumed 0x%lx bytes)\n", endoff - data_offset);
            CHECK_ERROR( endoff - data_offset != compressed_size, "consumed unexpected number of bytes" );

            data_offset = endoff;

        }

        printf("\nuncompressed stuff starting around 0x%08lx (if any)\n", data_offset);

#if 0
        /* dump 'mstr' segment if there's one */
        {
            long mstr_segment_size = read_32_le(&header_buf[0x1F8]);

            if ( 0 != mstr_segment_size )
            {
                const char file_name[] = "dumpUCmstr.bin";
                FILE *outfile = fopen(file_name, "wb");
                CHECK_ERRNO(NULL == outfile, file_name);

                const long pre_alignment_offset = data_offset;
                data_offset = (data_offset + uncompressed_alignment - 1) / uncompressed_alignment * uncompressed_alignment;

                printf("dumping uncompressed segment 'mstr'\n");
                printf("    (offset 0x%lx, 0x%lx bytes)\n",
                        data_offset, mstr_segment_size);

                /* check that the padding is zero-filled so we're not missing anything */
                {
                    for (long check_offset = pre_alignment_offset; check_offset < data_offset; check_offset ++)
                    {
                        CHECK_ERROR( 0 != get_byte_seek(check_offset, infile), "padding not zero-filled" );
                    }
                }

                dump(infile, outfile, data_offset, mstr_segment_size);

                CHECK_ERRNO(EOF == fclose(outfile), "fclose of output file");
                outfile = NULL;

                printf("    done!\n");

                data_offset += mstr_segment_size;
            }
        }
#endif

        /* handle uncompressed segments */

        int uncompressed_segment_number = 0;
        for (long uncompressed_segment_offset = 0x60; uncompressed_segment_offset < 0x84; uncompressed_segment_offset += 4)
        {

            /* handle uncompressed segment, if there is one */
            long uncompressed_segment_size = read_32_le(&header_buf[uncompressed_segment_offset]);
            if ( 0 != uncompressed_segment_size )
            {
                char file_name[128];
                snprintf(file_name,sizeof(file_name),"dumpUC%02d.bin", uncompressed_segment_number);
                FILE *outfile = fopen(file_name, "wb");
                CHECK_ERRNO(NULL == outfile, file_name);

                const long pre_alignment_offset = data_offset;
                data_offset = (data_offset + uncompressed_alignment - 1) / uncompressed_alignment * uncompressed_alignment;

                printf("dumping uncompressed segment %02x\n",
                        uncompressed_segment_number);
                printf("    (offset 0x%lx, 0x%lx bytes)\n",
                        data_offset, uncompressed_segment_size);

                /* check that the padding is zero-filled so we're not missing anything */
                {
                    for (long check_offset = pre_alignment_offset; check_offset < data_offset; check_offset ++)
                    {
                        CHECK_ERROR( 0 != get_byte_seek(check_offset, infile), "padding not zero-filled" );
                    }
                }

                dump(infile, outfile, data_offset, uncompressed_segment_size);

                CHECK_ERRNO(EOF == fclose(outfile), "fclose of output file");
                outfile = NULL;

                printf("    done!\n");

                data_offset += uncompressed_segment_size;

                uncompressed_segment_number ++;
            }
        }

    }

    CHECK_ERRNO(EOF == fclose(infile), "fclose");

    printf("done with %s!\n", infile_name);

    return 0;
}

/* returns end offset */
long decompress(
        FILE * const infile,
        FILE * const outfile,
        const long start_offset,
        const long bytes_to_decompress)
{
    long offset = start_offset;
    long start_output_offset = ftell(outfile);
    long output_offset = start_output_offset;

    CHECK_ERRNO(-1 == start_output_offset, "ftell");

    /* process a series of commands */
    do
    {
        uint8_t command_byte = get_byte_seek(offset ++, infile);

        if (command_byte & 0x80)
        {
            /* backreference */

            uint8_t byte2 = get_byte_seek(offset ++, infile);
            /* needs to make up for taking two bytes */
            uint8_t dupe_bytes = (byte2 & 0xf) + 2;

            /* bytes back to go for source */
            uint16_t src_rel = command_byte;
            src_rel = (src_rel << 4) | (byte2 >> 4);
            src_rel = (src_rel ^ 0xfff) + 2;

            long src_offset = output_offset - src_rel;

            CHECK_ERROR( src_offset < start_output_offset, "backreference before start of output" );
#if 0
            printf("%08lx-%08lx: backreference: %d bytes from %lx\n", 
                offset-2, output_offset, dupe_bytes, src_offset);
#endif

            unsigned char buf[0x11];    /* 0xf + 2 */
            get_bytes_seek(src_offset, outfile, buf, dupe_bytes);
            put_bytes_seek(output_offset, outfile, buf, dupe_bytes);

            output_offset += dupe_bytes;
        }
        else
        {
            if (command_byte & 0x40)
            {
                /* RLE */

                /* immediate + 2 (as RLE takes 2 bytes, no reason to bother if
                   less than 2 byte run) */
                uint8_t byte_count = (command_byte & 0x3f) + 2;
                uint8_t repeated_byte = get_byte_seek(offset ++, infile);
#if 0
                printf("%08lx-%08lx: RLE: %d bytes = of %02x\n",
                    offset-1, output_offset, byte_count, repeated_byte);
#endif
                for (int i = 0; i < byte_count; i++)
                {
                    put_byte(repeated_byte, outfile);
                }
                output_offset += byte_count;
            }
            else
            {
                /* literal */

                uint8_t byte_count = (command_byte & 0x3f) + 1;
#if 0
                printf("%08lx-%08lx: %d literal bytes\n",
                    offset-1, output_offset, byte_count);
#endif
                dump(infile, outfile, offset, byte_count);
                offset += byte_count;
                output_offset += byte_count;
            }
        }

        {
            long bytes_decompressed = output_offset - start_output_offset;
            CHECK_ERROR( bytes_decompressed > bytes_to_decompress, "decompressed too many bytes" );
            if (bytes_decompressed == bytes_to_decompress)
            {
                break;
            }
        }
    }
    while (1);

    return offset;
}
