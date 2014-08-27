#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include "guessfsb.h"
#include "error_stuff.h"
#include "util.h"

// FMOD fsb key guessing
// based on guessfsb 0.4


enum {FSB_TYPES = 2};
const int header_sizes[FSB_TYPES] = {0x18, 0x30};
const uint8_t fsb_magics[FSB_TYPES][4] = {"FSB3", "FSB4"};

struct guessfsb_state
{
    uint8_t swap_table[0x100];

    struct match_s
    {
        int length;
        uint8_t *key;
    } *matches;

    int match_count;

    uint8_t *indata;
    long file_size;
};

#if 0
static void analyze_header(const uint8_t * const indata,
                    long whole_file_size, long max_streams);
#endif

static int analyze_padding(struct guessfsb_state *s, good_key_callback_t *cb, void *cbv);

static int test_key(struct guessfsb_state *s, const uint8_t *key,
             const int key_length);

static int test_table(const uint8_t *indata, const uint8_t *key, const int key_length,
               const uint_fast32_t stream_count,
               const uint_fast32_t header_size,
               const uint_fast32_t table_size,
               const uint_fast32_t body_size);

static inline uint_fast32_t read_32_le_xor(const uint8_t *bytes, const long offset, const uint8_t *key, const int key_length);

static inline uint_fast16_t read_16_le_xor(const uint8_t *bytes, const long offset, const uint8_t *key, const int key_length);

static void print_key(const uint8_t *key, const int key_length, const int count);

static int process_matching_key(struct guessfsb_state *s, uint8_t const * key, long key_length);

static void decrypt_file(struct guessfsb_state *s, uint8_t const * key, long key_length);

// interface

int guess_fsb_keys(const uint8_t *infile, long file_size, good_key_callback_t *cb, void *cbv)
{
    int success = 0;
    struct guessfsb_state *s = malloc(sizeof(struct guessfsb_state));
    CHECK_ERRNO(!s, "malloc");

    // initialize bit swap table
    for (unsigned int i = 0; i < 0x100; i++)
    {
        s->swap_table[i] =
            (i&0x01) << 7 |
            (i&0x02) << 5 |
            (i&0x04) << 3 |
            (i&0x08) << 1 |
            (i&0x10) >> 1 |
            (i&0x20) >> 3 |
            (i&0x40) >> 5 |
            (i&0x80) >> 7;
    }

    s->indata = malloc(file_size);
    CHECK_ERRNO(!s->indata, "malloc for guessfsb indata");
    s->file_size = file_size;

    s->matches = NULL;
    s->match_count = 0;

    // swap everything up front
    for (long i=0; i < s->file_size; i++)
        s->indata[i] = s->swap_table[infile[i]];

    // guess!

    if (0 == analyze_padding(s, cb, cbv))
    {
        success = 1;
    }
    else
    {
        // do further checks
#if 0
        analyze_header(indata, file_size, max_streams);
#endif
    }

    // cleanup

    if (s->matches)
    {
        for (int i = 0; i < s->match_count; i++)
        {
            free(s->matches[i].key);
        }
        free(s->matches);
    }
    free(s->indata);
    free(s);

    if (success)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

#if 0
// By guessing values for the header, we can find keys < 16 bytes
void analyze_header(const uint8_t * const indata,
                    long whole_file_size, long max_streams_input)
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
                                process_matching_key(indata,
                                    key, key_length);
                            }
                        }   // end key length possibility
                    }   // end of key guess
                }   // end of table size guess
            }   // end of FSB type guess
        }   // end of stream count guess
    }   // done guessing
}
#endif

// test a key on a FSB file, 0 for failure, 1 for success
// set whole_file_size to 0 to skip file size check
int test_key(struct guessfsb_state *s, const uint8_t *key,
             const int key_length)
{
    uint8_t fsb_magic[4];
    uint_fast32_t stream_count, header_size, table_size, body_size;

    // attempted decrypted magic
    for (int i=0;i<4;i++)
        fsb_magic[i] = key[i%key_length] ^ s->indata[i];

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

    stream_count = read_32_le_xor(s->indata, 4, key, key_length);
    table_size = read_32_le_xor(s->indata, 8, key, key_length);
    body_size = read_32_le_xor(s->indata, 0xc, key, key_length);

    if (s->file_size != 0 &&
        header_size + table_size + body_size != s->file_size) return 0;
    if (stream_count < 1) return 0;
    if (stream_count * 0x28 > table_size) return 0;

    return test_table(s->indata, key, key_length, stream_count, header_size,
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

// Assume a constant byte for padding somewhere in the file
static int analyze_padding(struct guessfsb_state *s, good_key_callback_t *cb, void *cbv)
{
    printf("Trying padding...\n"
           "This should complete in less than a second, if not you should\n"
           "probably give up.\n\n");

    int byte_count[0x100] = {0};
    enum {max_key_length = 0x40};
    enum {min_key_length = 0x10};

    for (long repeat_end = 0; repeat_end < s->file_size; repeat_end++)
    {
        if (0 != byte_count[s->indata[repeat_end]])
        {
            // check for match ending here
            for (long repeat_length = min_key_length;
                    repeat_length <= max_key_length;
                    repeat_length++)

            {

                long checked_length = 0;
                for (long check_offset = repeat_end;
                        checked_length < repeat_length &&
                        check_offset-repeat_length > 0 &&
                        s->indata[check_offset] == s->indata[check_offset-repeat_length];
                        checked_length++, check_offset--) ;

                if (checked_length == repeat_length)
                {
                    int key_length = repeat_length;

                    uint8_t key[max_key_length];

                    // assume that repeated bytes are constant padding
                    for (int pad=0; pad <= 255; pad++)
                    {
                        for (int i=0; i < key_length; i++)
                        {
                            key[(repeat_end-i)%key_length] = 
                                s->indata[repeat_end-i] ^ pad;
                        }

                        if (test_key(s, key, key_length))
                        {
                            if (process_matching_key(s, key, key_length))
                            {
                                // decrypt file in place to give to callback
                                decrypt_file(s, key, key_length);

                                // invoke callback
                                if (0 == cb(s->indata, s->file_size, cbv))
                                {
                                    // callback was satisfied with the file
                                    return 0;
                                }

                                printf("Trying padding again...\n");
                                // reencrypt so we can continue
                                decrypt_file(s, key, key_length);
                            }
                        }
                    } // end loop through paddings
                } // end repeat detected
            } // end repeat length loop
        } // end if we've seen this character

        if (repeat_end-min_key_length+1 >= 0)
        {
            byte_count[s->indata[repeat_end-min_key_length+1]] ++;
        }

        if (repeat_end-max_key_length*2 >= 0)
        {
            byte_count[s->indata[repeat_end-max_key_length*2]] --;
        }
    }

    return 1;
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
    printf("\"\n\n");
    fflush(stdout);
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
static int matched_already(struct guessfsb_state *s, uint8_t const * key, long key_length)
{
    // check that we're not repeating keys
    for (int i=0; i < s->match_count; i++)
    {
        // if the new match is a multiple of the old match's length
        if (key_length >= s->matches[i].length &&
            key_length % s->matches[i].length == 0)
        {
            int j;
            // check that it isn't just repetition
            for (j=0; j < key_length / s->matches[i].length; j++)
            {
                if ( memcmp(s->matches[i].key, key + j*s->matches[i].length, 
                        s->matches[i].length) ) break;
            }
            if ( j == key_length / s->matches[i].length ) return 1;
        }
    }

    return 0;
}

// return 1 if unique, 0 otherwise
static int process_matching_key(struct guessfsb_state *s,
              uint8_t const * key, long key_length)
{

    if (matched_already(s, key, key_length)) return 0;

    s->match_count ++;
    s->matches = realloc(s->matches, s->match_count*sizeof(struct match_s));
    s->matches[s->match_count-1].key = malloc(key_length);
    CHECK_ERRNO(!s->matches[s->match_count-1].key, "malloc");
    memcpy(s->matches[s->match_count-1].key, key, key_length);
    s->matches[s->match_count-1].length = key_length;

    print_key(key, key_length, s->match_count);

    return 1;
}

static void decrypt_file(struct guessfsb_state *s, uint8_t const * key, long key_length)
{
    for (long i = 0; i < s->file_size; i++)
    {
        s->indata[i] ^= key[i%key_length];
    }
}
