#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

//#define BIG_ENDIAN

static void writeint32LE(uint8_t *c, uint32_t val)
{
    c[0] = val & 0xFF;
    val >>= 8;
    c[1] = val & 0xFF;
    val >>= 8;
    c[2] = val & 0xFF;
    val >>= 8;
    c[3] = val & 0xFF;
}

static void writeint16LE(uint8_t *c, uint32_t val)
{
    c[0] = val & 0xFF;
    val >>= 8;
    c[1] = val & 0xFF;
}

#ifdef BIG_ENDIAN
const unsigned int TYPE_OFFSET = 2;
const unsigned int SUBTYPE_OFFSET = 0;

static uint16_t readint16(const uint8_t *c)
{
    uint16_t total = c[0];
    total *= 0x100;
    total += c[1];
    return total;
}

static uint32_t readint32(const uint8_t *c)
{
    uint32_t total = c[0];
    total *= 0x100;
    total += c[1];
    total *= 0x10000;
    total += readint16(c+2);
    return total;
}
#else
const unsigned int TYPE_OFFSET = 0;
const unsigned int SUBTYPE_OFFSET = 2;

static uint16_t readint16(const uint8_t *c)
{
    uint16_t total = c[1];
    total *= 0x100;
    total += c[0];
    return total;
}

static uint32_t readint32(const uint8_t *c)
{
    uint32_t total = c[3];
    total *= 0x100;
    total += c[2];
    total *= 0x10000;
    total += readint16(c);
    return total;
}
#endif

enum EDataType
{
    eDataType_aud = 0,
    eDataType_unk = 1,
    eDataType_unk2 = 2,
    eDataType_unk3 = 3,
    eDataType_unk4 = 4,
    eDataType_sub = 5,
    eDataType_vid = 6,

    NUM_DATA_TYPES
};

// TODO: need a way to support multiple types with different subtype in one file (??? does this)

const char *datatypename[NUM_DATA_TYPES] = {"audio", "unknown", "unknown 2", "unknown 3", "unknown 4", "subtitles", "video"};
const char *datatypeext[NUM_DATA_TYPES] = {"mtaf", "dat", "dat2", "sub", "mpg"};
int usetype[NUM_DATA_TYPES] = {1, 0, 0, 0, 0, 0, 0};
int firstblock[NUM_DATA_TYPES] = {0};
int dump_file(FILE *infile, FILE *outfile, unsigned long size);
FILE *open_file(const char * prefix, const char * type_name, uint16_t sub_type, int file_number);

int main(int argc, char **argv)
{
    const char *infile_name = argv[1];
    const char *outfile_prefix = infile_name;
    FILE *infile = fopen(infile_name, "rb");
    uint8_t buf[0x10];

    if (argc == 3)
    {
        outfile_prefix = argv[2];
    }

    int file_number = 1;

    unsigned long cur_off;
    if (!infile) return 1;

    FILE *outfiles[NUM_DATA_TYPES] = {NULL};

    /* read into and out of "files" */
    while (8 == fread(buf, 1, 8, infile))
    {
        int got_oddness = 0;

        cur_off = ftell(infile) - 8;

        // size includes header
        const uint32_t block_size = readint32(&buf[4]);
        const uint16_t block_type = readint16(&buf[0+TYPE_OFFSET]);
        const uint16_t block_subtype = readint16(&buf[0+SUBTYPE_OFFSET]);

        enum EDataType eDataType = -1;

        switch (block_type)
        {
            case 0xF0:
                // end
                if (block_size != 0x10)
                {
                    fprintf(stderr, "end block not size 0x10 at 0x%lx\n", cur_off);
                    return 1;
                }

                if (8 != fread(buf, 1, 8, infile))
                {
                    fprintf(stderr, "error reading end block at 0x%lx\n", cur_off);
                    return 1;
                }

                {
                    unsigned long end_offset = ftell(infile);
                    unsigned long padded_offset = (end_offset+0x7ff)/0x800*0x800;
                    //printf("end at %08lx, pad to %08lx\n", end_offset, padded_offset);
                    fseek(infile, padded_offset - end_offset, SEEK_CUR);
                }

                for (int i = 0; i < NUM_DATA_TYPES; i++)
                {
                    if (outfiles[i])
                    {
                        fclose(outfiles[i]);
                        outfiles[i] = NULL;
                    }
                }

                file_number ++;

                printf("\n");

                break;

            case 0x10:
            {
                enum EDataType eDataType = -1;
                // metadata start
                unsigned long cur_off = ftell(infile);

                if (block_size != 0x10)
                {
                    fprintf(stderr, "metadata block not size 0x10 at 0x%lx\n", cur_off);
                    return 1;
                }

                if (8 != fread(buf, 1, 8, infile))
                {
                    fprintf(stderr, "error reading content descriptor at 0x%lx\n", cur_off);
                    return 1;
                }

                if (0 != readint32(&buf[0]))
                {
                    fprintf(stderr, "expected zero in content descriptor at 0x%lx\n", cur_off);
                    return 1;
                }

                const uint16_t block_type = readint16(&buf[4+TYPE_OFFSET]);
                const uint16_t block_subtype = readint16(&buf[4+SUBTYPE_OFFSET]);

                switch (block_type)
                {
                    case 1:
                        eDataType = eDataType_aud;
                        break;
                    case 2:
                        eDataType = eDataType_unk;
                        break;
                    case 4:
                        eDataType = eDataType_sub;
                        break;
                    case 5:
                        eDataType = eDataType_unk3;
                        break;
                    case 6:
                        eDataType = eDataType_unk4;
                        break;
                    case 0xe:
                        eDataType = eDataType_vid;
                        break;
                    case 0xf:
                        eDataType = eDataType_unk2;
                        break;

                    default:
                        fprintf(stderr, "unknown content descriptor %08lx at 0x%08lx\n", (unsigned long)readint32(&buf[4]), cur_off);
                        return 1;
                }

                printf("file %d at 0x%08lx has %s (subtype %d)\n", file_number, cur_off, datatypename[eDataType], readint16(&buf[6]));

                firstblock[eDataType] = 1;

                if (usetype[eDataType])
                {
                    if (outfiles[eDataType])
                    {
                        fprintf(stderr, "%s file is open, but another appeared at %lx\n", datatypename[eDataType], cur_off);
                        return 1;
                    }

                    if (eDataType == eDataType_aud && block_subtype == 16)
                    {
                        outfiles[eDataType] = open_file(outfile_prefix, "vag", block_subtype, file_number);
                    }
                    else if (eDataType == eDataType_aud && block_subtype == 1)
                    {
                        outfiles[eDataType] = open_file(outfile_prefix, "mta2", block_subtype, file_number);
                    }
                    else if (eDataType == eDataType_aud && block_subtype == 17)
                    {
                        outfiles[eDataType] = open_file(outfile_prefix, "mtaf", block_subtype, file_number);
                    }
                    else
                    {
                        outfiles[eDataType] = open_file(outfile_prefix, datatypeext[eDataType], block_subtype, file_number);
                    }

                    if (!outfiles[eDataType])
                    {
                        perror("failed opening output");
                        return 1;
                    }
                }

                break;
            }


            case 0xF:
                eDataType = eDataType_unk2;
                break;
            case 0xE:
                eDataType = eDataType_vid;
                break;
            case 0x5:
                eDataType = eDataType_unk4;
                break;
            case 0x6:
                eDataType = eDataType_unk3;
                break;
            case 0x4:
                eDataType = eDataType_sub;
                break;
            case 0x2:
                eDataType = eDataType_unk;
                break;
            case 0x1:
                eDataType = eDataType_aud;
                break;

            default:
                fprintf(stderr, "unknown block type %x at %lx\n", (unsigned int)readint16(&buf[0]), cur_off);
                return 1;
        }

        if (eDataType != -1)
        {
            if (usetype[eDataType])
            {
                int dump_this_block = 1;

                if (!outfiles[eDataType])
                {
                    fprintf(stderr, "hit %s data, but no stream was opened for it\n", datatypename[eDataType]);
                    return 1;
                }

                if (1 != fread(buf, 8, 1, infile))
                {
                    fprintf(stderr, "dump failed on reading thingy at %lx\n", cur_off);
                    return 1;
                }

                if (eDataType == eDataType_aud && (block_subtype == 1 || block_subtype == 17))
                {
                    // check for padding
                    if (readint32(&buf[0]) != 0)
                    {
                        fprintf(stderr, "first word of thingy was not zero at %lx\n", cur_off);
                        return 1;
                    }

                    if (readint32(&buf[4]) == 0 && !firstblock[eDataType])
                    {
                        // padding
                        dump_this_block = 0;
                    }
                }

                if (dump_this_block)
                {
                    if (dump_file(infile, outfiles[eDataType], block_size-16))
                    {
                        fprintf(stderr, "dump failed on file block at %08lx\n", cur_off);
                        return 1;
                    }
                }
                else
                {
                    fseek(infile, block_size-16, SEEK_CUR);
                }

            }
            else
            {
                fseek(infile, block_size-8, SEEK_CUR);
            }

            firstblock[eDataType] = 0;
        }

        if (ftell(infile) - cur_off != block_size && block_type != 0xF0)
        {
            fprintf(stderr, "didn't exactly use the block at %lx, expected %lx and read %lx\n", cur_off, (unsigned long)block_size, (unsigned long)(ftell(infile) - cur_off));
            return 1;
        }
    }

    printf("done!\n");

    return 0;

}

int dump_file(FILE *infile, FILE *outfile, unsigned long size)
{
    unsigned char buffer[0x200];

    for (;size >= sizeof(buffer);size -= sizeof(buffer))
    {
        if (1 != fread(buffer, sizeof(buffer), 1, infile))
        {
            fprintf(stderr, "error reading for dump\n");
            return 1;
        }

        if (1 != fwrite(buffer, sizeof(buffer), 1, outfile))
        {
            perror("fwrite");
            fprintf(stderr, "error writing dump\n");
            return 1;
        }
    }

    if (size > 0)
    {
        if (1 != fread(buffer, size, 1, infile))
        {
            fprintf(stderr, "error reading for dump\n");
            return 1;
        }

        if (1 != fwrite(buffer, size, 1, outfile))
        {
            perror("fwrite");
            fprintf(stderr, "error writing dump\n");
            return 1;
        }
    }

    return 0;
}

FILE *open_file(const char * prefix, const char * type_ext, uint16_t subtype, int file_number)
{
    size_t namelen = strlen(prefix) + 1 + 5 + 1 + 4 + 1 + strlen(type_ext) + 1;
    char * namebuf = malloc(namelen);

    if (!namebuf)
    {
        return 0;
    }

    snprintf(namebuf, namelen, "%s_%05u_%04x.%s\n", prefix, (unsigned int)file_number, (unsigned int)subtype, type_ext);
    FILE *outfile = fopen(namebuf, "wb");

    free(namebuf);

    return outfile;
}
