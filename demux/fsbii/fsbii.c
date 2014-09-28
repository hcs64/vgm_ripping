#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

/* fsbii 0.8 - convert multi-stream fsb into single-stream fsbs, or extract embedded fsbs */

#define CHECK(x,msg) \
    do { \
        if (x) { \
            fprintf(stderr, "error " msg "\n"); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define CHECK_ERRNO(x,msg) \
    do { \
        if (x) { \
            perror("error " msg); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define CHECK_FILE(x,file,msg) \
    do { \
        if (x) { \
            int temperrno = errno; \
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
        if ( whole_file_size - total_size > 0x800 )
        {
            /* maybe embedded? */
            return 0;
        }

        if (stream_count == 1)
        {
            printf("Already a single stream.\n");
            return 1;
        }

        printf("%" PRId32 " streams\n", stream_count);
    }

    /* copy each stream */
    {
        long table_offset = header_size;
        long body_offset = header_size + table_size;

        static const int max_name = 0x1e;
        static const char fsbext[] = ".fsb";
        int count_digits = ceil(log10(stream_count+2)); /* +1 since we add one, +1 to round up even counts */
        int name_bytes = count_digits + 1 + max_name + sizeof(fsbext);
        char *name_buf = malloc(name_bytes);
        CHECK_ERRNO(name_buf == NULL, "malloc for name buffer");

        for (int i = 0; i < stream_count; i++) {
            int16_t entry_size;
            int16_t padding_size;
            int32_t entry_file_size;
            const int entry_min_size = 0x28;
            unsigned char entry_buf[0x28];
            unsigned char pad_buf[0x10] = {0};
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

            /* build the output name */
            memset(name_buf, 0, name_bytes);
            snprintf(name_buf, count_digits+2, "%0*u_",
                    count_digits, (unsigned int)(i+1));
            memcpy(name_buf+count_digits+1, entry_buf+2, max_name);

            /* append .fsb to name */
            memcpy(name_buf+strlen(name_buf),fsbext,sizeof(fsbext));

            printf("%-*s"
                       " header 0x%02" PRIx32
                       " entry 0x%04" PRIx16 " (+0x%04" PRIx16 " padding)"
                       " body 0x%08" PRIx32
                       " (0x%08" PRIx32 ")\n",
                       name_bytes,
                       name_buf,
                       (uint32_t)header_size,
                       (uint16_t)entry_size,
                       (uint16_t)padding_size,
                       (uint32_t)entry_file_size,
                       (uint32_t)body_offset);

            /* open output */
            outfile = fopen(name_buf, "wb");
            CHECK_ERRNO(outfile == NULL, "opening output file");

            /* fill in the header */
            write32bitLE(1, &header[0x4]);
            write32bitLE(entry_size+padding_size, &header[0x8]);
            write32bitLE(entry_file_size, &header[0xc]);

            /* write out the header */
            size_rc = fwrite(header, 1, header_size, outfile);
            CHECK_FILE(size_rc != header_size, outfile, "writing header");

            /* write out the table entry */
            copy_bytes(infile, outfile, table_offset, entry_size);

            /* write out padding */
            if (padding_size) {
                size_rc = fwrite(pad_buf, 1, padding_size, outfile);
                CHECK_FILE(size_rc != padding_size, outfile, "writing pad");
            }

            /* write out the body */
            copy_bytes(infile, outfile, body_offset, entry_file_size);

            /* close */
            rc = fclose(outfile);
            CHECK_ERRNO(rc != 0, "closing output file");

            table_offset += entry_size;
            body_offset += entry_file_size;
            if (entry_file_size & 0x1F) {
                body_offset += 0x20 - (entry_file_size & 0x1F);
            }
        }

        free(name_buf);
    }

    return 1;
}

int try_embedded_fsb(FILE *infile)
{
    int rc;
    size_t size_rc;
    long whole_file_size;
    unsigned char search_buf[0x800];
    int gotone = 0;

    printf("\nTrying embedded search...\n");

    /* get file size */
    {
        rc = fseek(infile, 0, SEEK_END);
        CHECK_ERRNO(rc == -1, "seeking to file end");

        whole_file_size = ftell(infile);
        CHECK_ERRNO(whole_file_size == -1, "getting file size");
    }

    /* scan */
    for (long offset=0; offset<whole_file_size; offset+=sizeof(search_buf)-maxheadsize)
    {

        rc = fseek(infile, offset, SEEK_SET);
        CHECK_ERRNO(rc == -1, "seeking for search");
 
        if (offset + sizeof(search_buf) > whole_file_size)
        {
            size_rc = fread(search_buf, 1, whole_file_size-offset, infile);
            CHECK_FILE(size_rc != whole_file_size-offset, infile, "reading for search");
        }
        else
        {
            size_rc = fread(search_buf, 1, sizeof(search_buf), infile);
            CHECK_FILE(size_rc != sizeof(search_buf), infile, "reading for search");
        }

        for (long suboffset=0; suboffset<=sizeof(search_buf)-maxheadsize; suboffset++)
        {
            int32_t fsb_size;
            if (test_fsb_header(search_buf+suboffset,&fsb_size) && offset+suboffset+fsb_size <= whole_file_size)
            {
                /* found! */
                printf("found FSB 0x%08" PRIx32 " size 0x%08" PRIx32 "\n",
                        (uint32_t)(offset+suboffset), (uint32_t)fsb_size);

                gotone = 1;

                /* create filename */
                char name[30];
                snprintf(name,sizeof(name),"embedded_%08" PRIx32 ".fsb",(uint32_t)(offset+suboffset));

                FILE *outfile = fopen(name,"wb");
                CHECK_ERRNO(outfile == NULL, "opening output file");

                copy_bytes(infile, outfile, offset+suboffset, fsb_size);

                rc = fclose(outfile);
                CHECK_ERRNO(rc != 0, "closing output file");

                /* skip the contents */
                offset += fsb_size - (sizeof(search_buf)-maxheadsize);
            }
        }
    }

    return gotone;
}

int main(int argc, char *argv[])
{
    FILE *infile;
    int rc;
    if (argc != 2)
    {
        printf("fsbii 0.8 - convert multi-stream fsb into single-stream fsbs, or extract embedded fsbs\n"
                "usage: fsbii blah.fsb\n");
        exit(EXIT_FAILURE);
    }

    infile = fopen(argv[1],"rb");
    CHECK_ERRNO(infile == NULL, "opening input");

    if (!try_multistream_fsb(infile) && !try_embedded_fsb(infile))
    {
        printf("Sorry, couldn't make any sense of this file.\n");
        exit(EXIT_FAILURE);
    }

    rc = fclose(infile);
    CHECK_ERRNO(rc != 0, "closing input file");

    printf("Success!\n");

    exit(EXIT_SUCCESS);
}

