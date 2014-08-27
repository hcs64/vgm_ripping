#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include "error_stuff.h"
#include "util.h"


// guessfsb 0.3 - fmod .fsb decryption key guesser

enum {FSB_TYPES = 2};
const int header_sizes[FSB_TYPES] = {0x18, 0x30};
const uint8_t fsb_magics[FSB_TYPES][4] = {"FSB3", "FSB4"};

uint8_t swap_table[0x100];

struct match_s
{
    int length;
    uint8_t *key;
} *matches = NULL;

int match_count = 0;

uint8_t SwapByteBits(uint8_t cInput);

void init_swap_table(void);

void analyze_header(const uint8_t * const indata,
                    long whole_file_size, long max_streams, FILE *outfile);

void analyze_tail(const uint8_t * const indata, long whole_file_size,
                  FILE *outfile);

int test_key(const uint8_t *indata, long whole_file_size, const uint8_t *key,
             const int key_length);

int test_table(const uint8_t *indata, const uint8_t *key, const int key_length,
               const uint_fast32_t stream_count,
               const uint_fast32_t header_size,
               const uint_fast32_t table_size,
               const uint_fast32_t body_size);

void print_key(const uint8_t *key, const int key_length, const int count);

static inline uint_fast32_t read_32_le_xor(const uint8_t *bytes, const long offset, const uint8_t *key, const int key_length);

static inline uint_fast16_t read_16_le_xor(const uint8_t *bytes, const long offset, const uint8_t *key, const int key_length);

int process_matching_key(const uint8_t *indata, long whole_file_size,
              uint8_t const * key, size_t key_length, FILE *outfile);

int main(int argc, char **argv)
{
    printf("guessfsb 0.3\n");

    int max_streams = 0;

    const char *infile_name = NULL;
    const char *outfile_name = NULL;

    if (argc != 2)
    {
        printf("usage: guessfsb infile.fsb\n");
        exit(EXIT_FAILURE);
    }

    infile_name = argv[1];

    FILE *infile = fopen(infile_name,"rb");
    CHECK_ERRNO(!infile, "error opening infile");

    FILE *outfile = NULL;
    if (outfile_name)
    {
        outfile = fopen(outfile_name,"wb");
        CHECK_ERRNO(!outfile, "error opening outfile");
    }

    // get input file size
    CHECK_ERRNO(-1 == fseek(infile, 0, SEEK_END), "fseek");
    const long file_size = ftell(infile);
    CHECK_ERRNO(-1 == file_size, "ftell");

    init_swap_table();

    uint8_t *indata = malloc(file_size);
    CHECK_ERRNO(!indata, "malloc");

    // dump whole file
    get_bytes_seek(0, infile, indata, file_size);

    CHECK_ERRNO(EOF == fclose(infile), "fclose infile");

    // swap everything up front
    for (long i=0; i < file_size; i++)
        indata[i] = swap_table[indata[i]];

    analyze_tail(indata, file_size, outfile);

    analyze_header(indata, file_size, max_streams, outfile);

    free(indata); indata = NULL;

    printf("done!\n");
}

// By guessing values for the header, we can find keys < 16 bytes
void analyze_header(const uint8_t * const indata,
                    long whole_file_size, long max_streams_input, FILE *outfile)
{
    enum {EASY_HEADER_SIZE = 16};   // We can make good guesses for these
    uint8_t key[EASY_HEADER_SIZE];
    uint8_t guessed_header[EASY_HEADER_SIZE];

    // begin guessing
    {
        // Guess a stream count, starting from 1
        // (max out at file size/0x28 which would be the whole file filled
        // with minimal table entries)

        uint_fast32_t max_stream_count = whole_file_size / 0x28;

        if (0 != max_streams_input )
            max_stream_count = max_streams_input;
        
        printf("Trying headers with stream counts from 1 to %"PRIdFAST32"...\n",
                max_stream_count);
        fflush(stdout);

        for (uint_fast32_t stream_count = 1;
                stream_count <= max_stream_count;
                stream_count ++ )
        {
            if (stream_count % 20 == 0)
                printf("trying %" PRIuFAST32 " streams\n", stream_count);
            fflush(stdout);

            // Guess an FSB type
            for (int fsb_type = 0; fsb_type < FSB_TYPES; fsb_type++)
            {
                // We know the first 4 bytes (Magic)

                memcpy(guessed_header, fsb_magics[fsb_type], 4);
                for (int i=0;i<4;i++) key[i] = indata[i] ^ guessed_header[i];

                // Guess a table size, from 0x28*streams to 0xffff*streams

                uint_fast32_t max_table_size = UINT32_MAX;

                if (UINT64_C(0xffff)*stream_count < max_table_size)
                {
                    max_table_size = 0xffff*stream_count;
                }

                for (uint_fast32_t table_size = 0x28*stream_count;
                        table_size <= max_table_size;
                        table_size ++ )
                {
                    // From the file, header, and table size, compute the
                    // body size

                    uint_fast32_t body_size =
                        whole_file_size - header_sizes[fsb_type] - table_size;

                    // If the key is < 16 bytes this will fill it in
                    write_32_le(stream_count, &guessed_header[4]);
                    write_32_le(table_size, &guessed_header[8]);
                    write_32_le(body_size, &guessed_header[0xc]);

                    for (int i=4;i<EASY_HEADER_SIZE;i++)
                        key[i] = indata[i] ^ guessed_header[i];

                    // look for a repeat (could be several or none)

                    for (int key_length=1; key_length < EASY_HEADER_SIZE;
                            key_length++)
                    {
                        int key_length_check;
                        for (key_length_check=key_length;
                             key_length_check < EASY_HEADER_SIZE &&
                             key[key_length_check % key_length] ==
                             key[key_length_check];
                             key_length_check++) ;

                        // use the repeat length as the key size

                        if (key_length_check == EASY_HEADER_SIZE)
                        {
                            // consistency check on the header table
                            if (test_table (indata, key, key_length,
                                        stream_count, header_sizes[fsb_type],
                                        table_size, body_size))
                            {
                                process_matching_key(indata, whole_file_size,
                                    key, key_length, outfile);
                            }
                        }   // end key length possibility
                    }   // end of key guess
                }   // end of table size guess
            }   // end of FSB type guess
        }   // end of stream count guess
    }   // done guessing
}

// test a key on a FSB file, 0 for failure, 1 for success
// set whole_file_size to 0 to skip file size check
int test_key(const uint8_t *indata, long whole_file_size, const uint8_t *key,
             const int key_length)
{
    uint8_t fsb_magic[4];
    uint_fast32_t stream_count, header_size, table_size, body_size;

    // attempted decrypted magic
    for (int i=0;i<4;i++)
        fsb_magic[i] = key[i%key_length] ^ indata[i];

    int fsb_type;
    for (fsb_type=0; fsb_type < FSB_TYPES; fsb_type++)
    {
        if (!memcmp(fsb_magics[fsb_type], &fsb_magic[0], 4))
        {
            header_size = header_sizes[fsb_type];
            break;
        }
    }
    if (fsb_type == FSB_TYPES) return 0;

    stream_count = read_32_le_xor(indata, 4, key, key_length);
    table_size = read_32_le_xor(indata, 8, key, key_length);
    body_size = read_32_le_xor(indata, 0xc, key, key_length);

    if (whole_file_size != 0 &&
        header_size + table_size + body_size != whole_file_size) return 0;
    if (stream_count < 1) return 0;
    if (stream_count * 0x28 > table_size) return 0;

    return test_table(indata, key, key_length, stream_count, header_size,
            table_size, body_size);
}

// test a key on the header table, 0 for failure, 1 for success
int test_table(const uint8_t *indata, const uint8_t *key, const int key_length,
               const uint_fast32_t stream_count,
               const uint_fast32_t header_size,
               const uint_fast32_t table_size,
               const uint_fast32_t body_size)
{
    // 6a. We test each table entry, adding up the entry and
    // file sizes, stopping if

    uint_fast32_t entry_total = 0;
    uint_fast32_t file_total = 0;
    uint_fast32_t entry_offset = header_size;

    int file_num;

    for (file_num = 0; file_num < stream_count;
            file_num ++)
    {
        uint_fast16_t entry_size = 
            read_16_le_xor(indata, entry_offset,
                    key, key_length);
#if 0
        printf("[%d] entry_size = %"PRIxFAST16"\n",
                file_num,entry_size);
#endif

        // 6b. entry size < 0x28, entry size > table size
        if (entry_size < 0x28 || entry_size > table_size)
            return 0;

        uint_fast32_t entry_file_size =
            read_32_le_xor(indata, entry_offset + 0x24,
                    key, key_length);
#if 0
        printf("[%d] entry_file_size = %"PRIxFAST16"\n",
                file_num,entry_file_size);
#endif

        // 6c. file size > body size
        if (entry_file_size > body_size)
            return 0;

        entry_total += entry_size;
        file_total += entry_file_size;

        // 6d. total entry sizes > table size
        if (entry_total > table_size)
            return 0;

        // 6e. total file sizes > body size
        if (file_total > body_size)
            return 0;

        // :TODO:
        // 6f. check that file name is all
        //     printable characters?


        entry_offset += entry_size;
    }

#if 0
    printf("file_num = %d\n", file_num);
    printf("table_size = %"PRIxFAST32"\n", table_size);
    printf("body_size = %"PRIxFAST32"\n", body_size);
    printf("entry_total = %"PRIxFAST32"\n", entry_total);
    printf("file_total= %"PRIxFAST32"\n", file_total);
#endif


    // 7. Consistent in the end if
    //    table size - sum of all entries < 0x10
    //    body size - sum of all files < 0x800

    if ( file_num == stream_count &&
            table_size - entry_total <= 0x20 &&
            body_size - file_total < 0x800)
    {
        // :TODO:
        // 8. If we get something consistent with these
        //    checks, ensure that we actually got two
        //    checks for every byte in the key, otherwise
        //    mark key as suspect

        return 1;
    }

    return 0;
}

// Assume a constant byte for body tail padding
void analyze_tail(const uint8_t * const indata, long whole_file_size,
                  FILE *outfile)
{
    printf("Trying tail padding...\n");

    // find a starting point for a repeating sequence
    for (long repeat_start = whole_file_size-2, repeat_length=1;
            repeat_start > 0;
            repeat_start--, repeat_length++)
    {
        // check that the lower bytes match the bytes from here to the
        // end of the file
        long checked_length = 0;
        for (long check_offset = repeat_start;
                checked_length < repeat_length &&
                check_offset > 0 &&
                indata[check_offset] == indata[check_offset+repeat_length];
                checked_length++, check_offset--) ;

        if (checked_length == repeat_length)
        {
            if (repeat_length <= INT_MAX)
            {
                int key_length = repeat_length;

                uint8_t *key = malloc(key_length);

                // assume that repeated bytes are constant padding
                for (int pad=0; pad <= 255; pad++)
                {
                    for (int i=0; i < key_length; i++)
                    {
                        key[(repeat_start+i)%key_length] = 
                            indata[repeat_start+i] ^ pad;
                    }

                    if (test_key(indata, whole_file_size, key, key_length))
                    {
                        process_matching_key(indata, whole_file_size, key,
                            key_length, outfile);
                    }
                    else if (test_key(indata, 0, key, key_length))
                    {
                        if (process_matching_key(indata, whole_file_size, key,
                            key_length, outfile))
                        {
                            printf("^ Key decrypts consistent header, "
                               "but wrong file size. Multiple FSBs?\n");
                        }
                    }
                }

                free(key);
            }
            else
            {
                printf("possible key too long! %ld bytes!\n", repeat_length);
            }
        }
    }
}

void print_key(const uint8_t *key, const int key_length, const int count)
{
    printf("Possible key %d: ", count);
    for (int i=0;i<key_length;i++)
    {
        printf("%02" PRIx8 " ", key[i]);
    }
    printf(": ");

    printf("\"");
    for (int i=0;i<key_length;i++)
    {
        if (isprint(key[i]) &&
                key[i] != '"' &&
                key[i] != '\\')
            printf("%c", key[i]);
        else
            printf("\\x%" PRIx8, key[i]);
    }
    printf("\"\n");
    fflush(stdout);
}

// SwapByteBits from GHIII FSB Decryptor v1.0 by Invo
uint8_t SwapByteBits(uint8_t cInput)
{
   uint8_t nResult=0;

   for(int i=0; i<8; i++)
   {
      nResult = nResult << 1;
      nResult |= (cInput & 1);
      cInput = cInput >> 1;
   }

   return (nResult);
}

void init_swap_table(void)
{
    for (int i = 0; i < 0x100; i++)
    {
        swap_table[i] = SwapByteBits(i);
    }
}

static inline uint_fast32_t read_32_le_xor(const uint8_t *bytes, const long offset, const uint8_t *key, const int key_length)
{
    uint_fast32_t b1,b2,b3,b4;
    int key_offset = offset % key_length;
    b1 = bytes[offset + 0] ^ key[key_offset ++];
    if (key_offset == key_length) key_offset = 0;
    b2 = bytes[offset + 1] ^ key[key_offset ++];
    if (key_offset == key_length) key_offset = 0;
    b3 = bytes[offset + 2] ^ key[key_offset ++];
    if (key_offset == key_length) key_offset = 0;
    b4 = bytes[offset + 3] ^ key[key_offset];

    return 
        ((uint_fast32_t)b4) << 24 |
        ((uint_fast32_t)b3) << 16 |
        ((uint_fast32_t)b2) <<  8 |
        ((uint_fast32_t)b1);
}

static inline uint_fast16_t read_16_le_xor(const uint8_t *bytes, const long offset, const uint8_t *key, const int key_length)
{
    uint_fast16_t b1,b2;
    int key_offset = offset % key_length;
    b1 = bytes[offset + 0] ^ key[key_offset ++];
    if (key_offset == key_length) key_offset = 0;
    b2 = bytes[offset + 1] ^ key[key_offset];

    return 
        ((uint_fast32_t)b2) <<  8 |
        ((uint_fast32_t)b1);
}

// return 1 if unique, 0 otherwise
int process_matching_key(const uint8_t *indata, long whole_file_size,
              uint8_t const * key, size_t key_length, FILE *outfile)
{
    // check that we're not repeating keys
    for (int i=0; i < match_count; i++)
    {
        // if the new match is a multiple of the old match's length
        if (key_length >= matches[i].length &&
            key_length % matches[i].length == 0)
        {
            int j;
            // check that it isn't just repetition
            for (j=0; j < key_length / matches[i].length; j++)
            {
                if ( memcmp(matches[i].key, key + j*matches[i].length, 
                        matches[i].length) ) break;
            }
            if ( j == key_length / matches[i].length ) return 0;
        }
    }

    match_count ++;
    matches = realloc(matches, match_count*sizeof(struct match_s));
    matches[match_count-1].key = malloc(key_length);
    CHECK_ERRNO(!matches[match_count-1].key, "malloc");
    memcpy(matches[match_count-1].key, key, key_length);
    matches[match_count-1].length = key_length;

    print_key(key, key_length, match_count);

    return 1;
}
