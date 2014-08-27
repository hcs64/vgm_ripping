/* 
   LZH8 compressor

   An implementation of LZSS, with symbols stored via two Huffman codes:
   - one for backreference lengths and literal bytes (8 bits each)
   - one for backreference displacement lengths (bits - 1)

   Note that the goal here is to reproduce exactly the compression used in
   Nintendo Virtual Console games. I've tried to point out with "POLICY:"
   comments places where I have determined particular behavioral oddities of
   Nintendo's implementation.

   The hashing method used to speed up LZSS was suggested by
   Michael Dipperstein, on his LZSS discussion site:
      http://michael.dipperstein.com/lzss/
   I have used exactly the hash function he recommends, referenced from:
      K. Sadakane and H. Imai
      Improving the Speed of LZ77 Compression by Hashing and Suffix Sorting,
      IEICE Trans. Fundamentals, Vol. E83-A, No. 12, pp. 2689--2798, 2000.

   The method for flattening the Huffman tree is based on Nintendo's
   Huffman compressor for 8 and 4 bit (with 8 bit tables), by Makoto Takano.
   It is implemented a little differently in LZH8, however.
  
   All else by hcs.
   This software is released to the public domain as of November 2, 2009.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "util.h"
#include "error_stuff.h"

#define VERSION "0.8 "
#ifndef LZH8_NONSTRICT
#define BUILD_STRING VERSION __DATE__
#else
#define BUILD_STRING VERSION "nonstrict " __DATE__
#endif

/* debug output options */
#define SHOW_SYMBOLS        0
#define SHOW_FREQUENCIES    0
#define SHOW_TREE_BITS      0
#define SHOW_TREE           0
#define SHOW_TABLE          0
#define EXPLAIN_TABLE       0

/* dump of lzss stage output */
#define MAKE_DUMP           0
#define READ_DUMP           0

/* display percentage of LZSS progress on stderr */
#define REPORT_PROGRESS     1

/* Use hashes to generate LZSS faster */
#define LZSS_HASH           1

#ifndef LZH8_NONSTRICT
/* exactly reproduce Nintendo's output (slightly lower compression) */
#define STRICT_COMPRESSION  1
#else
#define STRICT_COMPRESSION  0
#endif

void LZH8_compress(FILE *infile, FILE *outfile, long file_length);

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("lzh8_cmp " BUILD_STRING "\n\n");
        printf("Usage: %s infile outfile\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* open file */
    FILE *infile = fopen(argv[1], "rb");
    CHECK_ERRNO(!infile, "fopen");

    FILE *outfile = fopen(argv[2], "wb");
    CHECK_ERRNO(!infile, "fopen");

    /* get file size */
    CHECK_ERRNO(fseek(infile, 0 , SEEK_END) != 0, "fseek");
    long file_length = ftell(infile);
    CHECK_ERRNO(file_length == -1, "ftell");

    rewind(infile);

    LZH8_compress(infile, outfile, file_length);

    CHECK_ERRNO(fclose(outfile) == EOF, "fclose");

    CHECK_ERRNO(fclose(infile) == EOF, "fclose");

    exit(EXIT_SUCCESS);
}

/* Constants */

enum {LENBITS = 9};
enum {DISPBITS = 5};
enum {LENCNT = ( 1 << LENBITS )};
enum {DISPCNT = ( 1 << DISPBITS )};

/* Structures */

struct lzss_symbol
{
    /* flag, 0 = literal, 1 = reference */
    uint8_t  is_reference;
    /* length of backreference -3, or literal byte */
    uint8_t length_or_literal;
    /* displacement of reference -1, unused if literal */
    uint16_t offset;
};

struct huff_node
{
    /* Indexes of children.
       -1 if no children, either both or neither should be -1 */
    int lchild, rchild;

    /* stored value for the code leading to this node, if a leaf (lchild=-1,
       rchild=-1), otherwise unused */
    uint16_t leaf;

    /* Number of nodes under this, thus table entries required to
       store the entire subtree. Importantly, this includes the cost of
       the root. 0 for leaves, as they do not get stored independently. */
    uint16_t subtree_size;
};

struct huff_table_ctrl
{
    /* index of associated huff_node */
    int node_idx;
    /* flag to show if what I point to has been placed */
    bool placed : 1;
};

struct huff_symbol
{
    /* length of prefix key */
    uint16_t key_len;
    /* bits of the key, MSBit->LSBit order, aligned to the LSBit end */
    uint32_t key_bits;
};

/* Prototypes */

int LZH8_displen_length(uint16_t displacement);

/* LZSS prototypes */

void LZH8_LZSS_compress(
        unsigned char *input_data,
        long input_length,
        struct lzss_symbol **lzss_stream_p,
        long *lzss_length_p);

/* Huffman prototypes */

void LZH8_Huff_produce_encodings(
        struct lzss_symbol * const lzss_stream,
        long lzss_length,
        struct huff_symbol *back_litlen_table,
        struct huff_symbol *back_displen_table,
        long *output_offset_p,
        FILE *outfile);

int LZH8_Huff_build_Huffman_tree(
    int *node_remains,
    long *freq,
    struct huff_node *node_array,
    int symbol_count);

void LZH8_Huff_compute_prefix(
        const struct huff_node *node_array,
        int root_idx,
        struct huff_symbol *sym_array,
        uint32_t key_bits,
        int key_len);

int LZH8_Huff_flatten_tree(
    const struct huff_node *node_array,
    uint16_t *tree_table,
    int root_idx,
    const int offset_bits);

void LZH8_Huff_flatten_single_node(
    const struct huff_node *node_array,
    struct huff_table_ctrl *ctrl,
    uint16_t *tree_table,
    const int offset_bits,
    unsigned int parent_idx,
    unsigned int *table_idx_p,
    unsigned int *outstanding_nodes_p);

bool LZH8_Huff_could_satisfy_outstanding_nodes(
    const struct huff_table_ctrl *ctrl,
    int table_idx,
    uint16_t proposed_size,
    int proposed_idx,
    const int offset_bits);

/* Bitstream writing, 4 byte blocks */

/* empty bit pool into outfile (if it wasn't empty already) */
static inline void flush_bits(
        FILE * const outfile,
        long * const offset_p,
        uint32_t * const bit_pool_p,
        int * const bits_written_p)
{
    if (0 != *bits_written_p)
    {
        put_32_be_seek(*bit_pool_p, *offset_p, outfile);
        *bits_written_p = 0;
        *bit_pool_p = 0;
        *offset_p += 4;
    }
}

/* write MSB->LSB order, 4 bytes at a time */
static inline void write_bits(
        FILE * const outfile,
        long * const offset_p,
        uint32_t * const bit_pool_p,
        int * const bits_written_p,
        const uint32_t bits_to_write,
        const int bit_count)
{
    int num_bits_produced = 0;
    CHECK_ERROR( bit_count > 32, "too many bits" );
#if SHOW_TREE_BITS
        printf("write ");
        for (int i=bit_count-1; i>=0; i--)
        {
            printf("%c", (bits_to_write&(1<<i)) ? '1' : '0');
        }
        printf("\n");
#endif
    while (num_bits_produced < bit_count)
    {
        if (32 == *bits_written_p)
        {
            flush_bits(outfile, offset_p, bit_pool_p, bits_written_p);
        }

        int bits_this_round;
        if (*bits_written_p + (bit_count - num_bits_produced) <= 32)
            bits_this_round = bit_count - num_bits_produced;
        else
            bits_this_round = 32 - *bits_written_p;

        uint32_t selected_bits = 
            bits_to_write >> (bit_count - bits_this_round - num_bits_produced);
        selected_bits &= ((1 << bits_this_round) - 1);
        *bit_pool_p |= selected_bits << (32 - bits_this_round - *bits_written_p);

        *bits_written_p += bits_this_round;
        num_bits_produced += bits_this_round;
    }
}

#define WRITE_BITS(bits_to_write, bit_count) \
    write_bits(outfile, output_offset_p, &bit_pool, &bits_written, bits_to_write, bit_count)

/* Main LZH8 compression function */

void LZH8_compress(FILE *infile, FILE *outfile, long file_length)
{
    long output_offset = 0;

    /*
       Step 0: Output header
    */
    {
        if (UINT32_C(0x1000000) > file_length && 0 != file_length)
        {
            put_32_le_seek( (((uint32_t)file_length) << 8) | 0x40,
                    output_offset, outfile );
            output_offset += 4;
        }
        else
        {
            /* >= 0x1000000 needs 4 extra bytes */

            CHECK_ERROR(file_length > UINT32_MAX, "input file is too large");

            put_32_le_seek( 0x40, output_offset, outfile );
            output_offset += 4;
            put_32_le_seek( file_length, output_offset, outfile );
            output_offset += 4;
        }
    }

    /*
       Step 1: LZSS with:
        reference length 3 <= length <= 2^8 - 1 + 3
        2^15 byte window
       Produce a series of symbols, literal byte and backreference length+offset
    */
    struct lzss_symbol *lzss_stream = NULL;
    long lzss_length = 0;    /* length in symbols */

#if !READ_DUMP
    {
        unsigned char *input_data = NULL;

        input_data = malloc(file_length);
        CHECK_ERRNO( NULL == input_data, "malloc" );

        get_bytes_seek(0, infile, input_data, file_length);

        LZH8_LZSS_compress(input_data, file_length, &lzss_stream, &lzss_length);

#if MAKE_DUMP
        // temp, dump LZSS stuff to file
        {
            FILE *lzss_dump = fopen("lzss.dump", "wb");
            CHECK_ERRNO(NULL == lzss_dump, "fopen");

            put_bytes(lzss_dump, (unsigned char *)lzss_stream,
                    lzss_length * sizeof(struct lzss_symbol));

            CHECK_ERRNO(EOF == fclose(lzss_dump), "fclose");
        }
#endif

        free(input_data);
    }
#else
    // temp, read LZSS stuff from file
    {
        FILE *lzss_dump = fopen("lzss.dump", "rb");
        CHECK_ERRNO(NULL == lzss_dump, "fopen");

        /* get dump size */
        CHECK_ERRNO(fseek(lzss_dump, 0 , SEEK_END) != 0, "fseek");
        long lzss_dump_length = ftell(lzss_dump);
        CHECK_ERRNO(lzss_dump_length == -1, "ftell");

        lzss_stream = malloc(lzss_dump_length);

        get_bytes_seek(0, lzss_dump,
                (unsigned char *)lzss_stream, lzss_dump_length);

        lzss_length = lzss_dump_length / sizeof(struct lzss_symbol);

        CHECK_ERRNO(EOF == fclose(lzss_dump), "fclose");
    }
#endif

    /*
       Step 2: Count frequencies and build Huffman codes, output flat trees
    */
    struct huff_symbol back_litlen_table[LENCNT];
    struct huff_symbol back_displen_table[DISPCNT];
    {
        LZH8_Huff_produce_encodings(lzss_stream, lzss_length, back_litlen_table,
                back_displen_table, &output_offset, outfile);
    }

    /*
       Step 3: Output the encoded symbol stream
    */
    {
        long * const output_offset_p = &output_offset;
        uint32_t bit_pool = 0;
        int bits_written = 0;

#if SHOW_SYMBOLS
        long data_offset = 0;
#endif
        for (long i=0; i < lzss_length; i++)
        {
            struct huff_symbol litlen_symbol =
                back_litlen_table[
                    (lzss_stream[i].is_reference << 8) |
                     lzss_stream[i].length_or_literal ];
#if SHOW_SYMBOLS
            printf("%08lx symbol %ld: ", (unsigned long)data_offset, i);
#endif
            WRITE_BITS(litlen_symbol.key_bits, litlen_symbol.key_len);

            if (lzss_stream[i].is_reference)
            {
                uint16_t offset = lzss_stream[i].offset;
                int displen_length = LZH8_displen_length(offset);

                struct huff_symbol displen_symbol =
                    back_displen_table[displen_length];
                WRITE_BITS(displen_symbol.key_bits, displen_symbol.key_len);

                if (offset > 1)
                {
                    WRITE_BITS(offset,displen_length-1);
                }
#if SHOW_SYMBOLS
                printf("%d bytes, offset %d\n",
                    (int)lzss_stream[i].length_or_literal + 3,
                    (int)lzss_stream[i].offset+1);
                data_offset += lzss_stream[i].length_or_literal + 3;
#endif
            }
#if SHOW_SYMBOLS
            else
            {
                printf("literal %02"PRIX8"\n",lzss_stream[i].length_or_literal);
                data_offset ++;
            }
#endif
        }

        flush_bits(outfile, output_offset_p, &bit_pool, &bits_written);
    }

    free(lzss_stream);
}


#if LZSS_HASH

/* LZSS with hashing */
static inline int LZSS_hash(unsigned char *input, int length, int hash_size)
{
    int key = 0;
    for (int i=0; i < length; i++)
    {
        key = (key << 5) ^ input[i];
        key %= hash_size;
    }

    return key;
}

void LZH8_LZSS_compress(
        unsigned char *input_data,
        long input_length,
        struct lzss_symbol **lzss_stream_p,
        long *lzss_length_p)
{
    /* POLICY: parameters for this coding */
    const int min_length = 3;
    const int max_length = (1 << 8) - 1 + 3;
#if STRICT_COMPRESSION
    const long max_window_size = (1l << 15);
#else
    const long max_window_size = (1l << 16);
#endif

    /* for speed: a hash table offsets of all 3-byte strings in the dict */
    struct lzss_hash_node
    {
        long offset;
        struct lzss_hash_node *next_node;
        struct lzss_hash_node *prev_node;
    };
    const int hash_size = 1024;

    struct lzss_hash_node *hash_queue =
        malloc(max_window_size * sizeof(struct lzss_hash_node));
    CHECK_ERRNO( NULL == hash_queue, "malloc" );
    /* insert at head of queue, remove from tail, wrap at max_window_size */
    unsigned int hash_queue_tail = 0, hash_queue_head = 0;

    /* heads of the linked list */
    struct lzss_hash_node *hash_table = 
        malloc(hash_size * sizeof(struct lzss_hash_node));
    CHECK_ERRNO( NULL == hash_table, "malloc" );
    for (int i=0; i < hash_size; i++)
    {
        hash_table[i].next_node = NULL;
        hash_table[i].prev_node = NULL;
        hash_queue[i].offset = -2;
    }
    for (long i=0; i < max_window_size; i++)
    {
        hash_queue[i].next_node = NULL;
        hash_queue[i].prev_node = NULL;
        hash_queue[i].offset = -1;
    }

    /* the output stream */
    struct lzss_symbol *lzss_stream = *lzss_stream_p;
    long lzss_length = *lzss_length_p;
    long lzss_stream_capacity = 0;
    CHECK_ERROR( NULL != lzss_stream || 0 != lzss_length,
            "should start with nothing");

    long bytes_done = 0;    /* bytes of input successfully encoded */
    long window_size = 0;   /* bytes (before bytes_done) of dictionary, also
                               length of queue (minimum symbols beginning at
                               each window offset) */
    long next_input_offset; /* next input byte to read */

#if REPORT_PROGRESS
    long last_report = -0x40000l;
#endif
    for (bytes_done = 0; bytes_done < input_length; )
    {
        int longest_match = 0;
        long longest_match_offset = 0;

#if REPORT_PROGRESS
        if (bytes_done - last_report >= 0x40000l)
        {
            fprintf(stderr,"%ld bytes done (%.0f%%)\n", bytes_done, (float)bytes_done/input_length*100);
            last_report = bytes_done;
        }
#endif
        
        /* grab as many input bytes as possible */
        next_input_offset = bytes_done + max_length;
        if (next_input_offset > input_length)
        {
            next_input_offset = input_length;
        }

        /* enough bytes for a match remaining in input? */
        if ( bytes_done + min_length <= input_length )
        {
            /* search for a match among strings with same hash as next
                min_length bytes, linked list is in most-recent-first order */
            int input_key =
                LZSS_hash(&input_data[bytes_done], min_length, hash_size);

            for (
                    struct lzss_hash_node *cur_match =
                        hash_table[input_key].next_node;
                    NULL != cur_match;
                    cur_match = cur_match->next_node)
            {
                long match_length = 0;

#if STRICT_COMPRESSION
                if ( cur_match->offset == bytes_done - 1 ) /* POLICY: not -1 */
                {
                    continue;
                }
#endif

                /* check bytes at the given offset */
                for (long match_check_offset = cur_match->offset,
                          input_check_offset = bytes_done;
                     input_check_offset < next_input_offset &&
                        input_data[input_check_offset] ==
                        input_data[match_check_offset];
                    input_check_offset ++, match_check_offset++, match_length++)
                {}

                /* POLICY: prefer long matches, then recent matches */
                if (match_length > longest_match)
                {
                    longest_match = match_length;
                    longest_match_offset = cur_match->offset;
                }
            }
        }

        /* check that there's room for a new symbol */
        if (lzss_length >= lzss_stream_capacity)
        {
            if (0 == lzss_stream_capacity)
                lzss_stream_capacity = 0x800;
            else
                lzss_stream_capacity *= 2;
            lzss_stream = realloc(lzss_stream,
                    lzss_stream_capacity*sizeof(struct lzss_symbol));
            CHECK_ERRNO( NULL == lzss_stream, "realloc" );
        }

        long bytes_in_this_symbol;

        /* record the new symbol */
        if (longest_match < min_length)
        {
            /* no backreference possible */
            lzss_stream[lzss_length].is_reference = 0;
            lzss_stream[lzss_length].length_or_literal = input_data[bytes_done];
            lzss_length++;
            bytes_in_this_symbol = 1;
        }
        else
        {
            /* generate a backreference */
            lzss_stream[lzss_length].is_reference = 1;
            lzss_stream[lzss_length].length_or_literal = longest_match - 3;
            lzss_stream[lzss_length].offset = bytes_done-longest_match_offset-1;
            lzss_length++;
            bytes_in_this_symbol = longest_match;
        }

        /* update state, new match entry for each byte processed */
        for (long i = 0; i < bytes_in_this_symbol;
                i++, bytes_done ++)
        {
            if (window_size == max_window_size)
            {
                /* discard oldest string */

                CHECK_ERROR( hash_queue_head != hash_queue_tail,
                    "queue should be full if we're removing an element");

                struct lzss_hash_node *old_node = &hash_queue[hash_queue_head];

                CHECK_ERROR( NULL != old_node->next_node,
                        "oldest node must be at end of list" );
                CHECK_ERROR( NULL == old_node->prev_node,
                        "oldest node must link from something" );
                CHECK_ERROR( old_node->offset == bytes_done + max_window_size,
                        "oldest node should be byte passing out of window" );

                /* kill previous node's link to me */
                old_node->prev_node->next_node = NULL;

                /* free space in queue */
                hash_queue_head = (hash_queue_head+1) % max_window_size;
                old_node->offset = -1;
                window_size --;
            }

            if (input_length - bytes_done >= min_length)
            {
                /* add string to hash table */

                CHECK_ERROR(0 != window_size &&
                        hash_queue_head == hash_queue_tail, "overflow");
                struct lzss_hash_node *new_node = &hash_queue[hash_queue_tail];
                int hash_key = LZSS_hash(&input_data[bytes_done], min_length,
                        hash_size);

                CHECK_ERROR( -1 != new_node->offset,
                        "new node wasn't marked unused" );

                /* put node on linked list*/
                new_node->next_node = hash_table[hash_key].next_node;
                new_node->prev_node = &hash_table[hash_key];
                hash_table[hash_key].next_node = new_node;
                if (NULL != new_node->next_node)
                {
                    /* next node needs to link back to me */
                    new_node->next_node->prev_node = new_node;
                }

                new_node->offset = bytes_done;

                /* mark space used */
                hash_queue_tail = (hash_queue_tail+1) % max_window_size;
                window_size ++;
            }
        }

    }

    *lzss_length_p = lzss_length;
    *lzss_stream_p = lzss_stream;

    free(hash_queue);
    free(hash_table);
}

#else

/* LZSS with dumb linear search */
void LZH8_LZSS_compress(
        unsigned char *input_data,
        long input_length,
        struct lzss_symbol **lzss_stream_p,
        long *lzss_length_p)
{
    /* POLICY: parameters for this coding */
    const int min_length = 3;
    const int max_length = (1 << 8) - 1 + 3;
#if STRICT_COMPRESSION
    const long max_window_size = (1l << 15);
#else
    const long max_window_size = (1l << 16);
#endif

    /* the output stream */
    struct lzss_symbol *lzss_stream = *lzss_stream_p;
    long lzss_length = *lzss_length_p;
    long lzss_stream_capacity = 0;
    CHECK_ERROR( NULL != lzss_stream || 0 != lzss_length,
            "should start with nothing");

    long bytes_done = 0;    /* bytes of input successfully encoded */
    long window_size = 0;   /* bytes (before bytes_done) of dictionary */
    long next_input_offset; /* next input byte to read */

    for (bytes_done = 0; bytes_done < input_length; )
    {
        int longest_match = 0;
        long longest_match_offset = 0;

#if REPORT_PROGRESS
        if (0 == lzss_length % (5*0x400))
        {
            fprintf(stderr,"%ld bytes done (%f%%)\n", bytes_done, (float)bytes_done/input_length*100);
        }
#endif
        
        /* grab as many input bytes as possible */
        next_input_offset = bytes_done + max_length;
        if (next_input_offset > input_length)
        {
            next_input_offset = input_length;
        }

        /* consider window */
        window_size = bytes_done;
        if (max_window_size < window_size)
        {
            window_size = max_window_size;
        }

        /* search for a match for what's currently in the input */
        for (long search_offset =
#if STRICT_COMPRESSION
                  bytes_done - 2,   /* POLICY: not -1 */
#else
                  bytes_done - 1,
#endif
                  search_end = bytes_done - 1 - window_size;
             search_offset > search_end;
             search_offset--)
        {
            long match_length = 0;
            for (long match_check_offset = search_offset,
                      input_check_offset = bytes_done;
                 input_check_offset < next_input_offset &&
                 input_data[input_check_offset] ==
                    input_data[match_check_offset];
                 input_check_offset ++, match_check_offset++, match_length++)
            {}

            /* POLICY: prefer long matches, then first matches */
            if (match_length > longest_match)
            {
                longest_match = match_length;
                longest_match_offset = search_offset;
            }
        }

        /* check that there's room for a new symbol */
        if (lzss_length >= lzss_stream_capacity)
        {
            if (0 == lzss_stream_capacity)
                lzss_stream_capacity = 0x800;
            else
                lzss_stream_capacity *= 2;
            lzss_stream = realloc(lzss_stream,
                    lzss_stream_capacity*sizeof(struct lzss_symbol));
            CHECK_ERRNO( NULL == lzss_stream, "realloc" );
        }

        long bytes_in_this_symbol;

        /* record the new symbol */
        if (longest_match < min_length)
        {
            /* no backreference possible */
            lzss_stream[lzss_length].is_reference = 0;
            lzss_stream[lzss_length].length_or_literal = input_data[bytes_done];
            lzss_length++;
            bytes_in_this_symbol = 1;
        }
        else
        {
            /* generate a backreference */
            lzss_stream[lzss_length].is_reference = 1;
            lzss_stream[lzss_length].length_or_literal = longest_match - 3;
            lzss_stream[lzss_length].offset = bytes_done-longest_match_offset-1;
            lzss_length++;
            bytes_in_this_symbol = longest_match;
        }

        /* update state */
        bytes_done += bytes_in_this_symbol;
    }

    *lzss_length_p = lzss_length;
    *lzss_stream_p = lzss_stream;
}
#endif

int LZH8_displen_length(uint16_t displacement)
{
    int bits = 0;
    while (displacement)
    {
        displacement >>= 1;
        bits ++;
    }

    return bits;
}

/* Build the Huffman code to be used for this file. Also produce the
   flattened tables and output them. */
void LZH8_Huff_produce_encodings(
        struct lzss_symbol * const lzss_stream,
        long lzss_length,
        struct huff_symbol *back_litlen_table,
        struct huff_symbol *back_displen_table,
        long *output_offset_p,
        FILE *outfile)
{
    /* Count frequencies */
    long length_freq[LENCNT*2-1] = {0};
    long displen_freq[DISPCNT*2-1] = {0};

    for (long i=0; i < lzss_length; i++)
    {
        length_freq[ (lzss_stream[i].is_reference << 8) |
                      lzss_stream[i].length_or_literal ] ++;

        if (lzss_stream[i].is_reference)
        {
            displen_freq[ LZH8_displen_length(lzss_stream[i].offset) ] ++;
        }
    }

#if SHOW_FREQUENCIES
    for (int i=0; i < LENCNT; i++)
    {
        printf("%d: %ld\n", i, length_freq[i]);
    }
    for (int i=0; i < DISPCNT; i++)
    {
        printf("%d: %ld\n", i, displen_freq[i]);
    }
#endif

    /* Build Huffman codes for length/literal */
    {
        int node_remains[LENCNT*2-1];
        struct huff_node node_array[LENCNT*2-1];
#if SHOW_TREE
        printf("\nlength/literal tree:\n");
#endif
        int root_idx = LZH8_Huff_build_Huffman_tree(
                node_remains, length_freq, node_array, LENCNT);

#if SHOW_TREE_BITS
        printf("\nlength/literal tree:\n");
#endif

        LZH8_Huff_compute_prefix(
                node_array, root_idx, back_litlen_table, 0, 0);

        uint16_t tree_table[LENCNT*2] = {0};
        int table_size =
            LZH8_Huff_flatten_tree(node_array, tree_table, root_idx, LENBITS-2);

        long start_output_offset = *output_offset_p;

        /* write bit packed table */
#if SHOW_TABLE
        printf("backreference length table:\n");
#endif
        uint32_t bit_pool = 0;
        int bits_written = 16;   /* leave space for length */
        for (int i=1; i < table_size; i++)
        {
#if SHOW_TABLE
            printf("%d: %d\n", i, (int)tree_table[i]);
#endif
            WRITE_BITS(tree_table[i], LENBITS);
        }
        flush_bits(outfile, output_offset_p, &bit_pool, &bits_written);

        long table_bytes = (*output_offset_p - start_output_offset) / 4 - 1;
        CHECK_ERROR( UINT16_MAX <= table_bytes, "length table too big" );
        put_16_le_seek(table_bytes, start_output_offset, outfile);

#if SHOW_TABLE
        printf("done at 0x%lx\n\n", *output_offset_p);
#endif
    }

    /* Build Huffman codes for displacement length */
    {
        int node_remains[DISPCNT*2-1];
        struct huff_node node_array[DISPCNT*2-1];
#if SHOW_TREE
        printf("\ndisplacement length tree:\n");
#endif
        int root_idx = LZH8_Huff_build_Huffman_tree(
                node_remains, displen_freq, node_array, DISPCNT);

#if SHOW_TREE_BITS
        printf("\ndisplacement length tree:\n");
#endif

        LZH8_Huff_compute_prefix(
                node_array, root_idx, back_displen_table, 0, 0);

        uint16_t tree_table[DISPCNT*2] = {0};
        int table_size =
            LZH8_Huff_flatten_tree(node_array, tree_table, root_idx,
            DISPBITS-2);

        /* write out table size placeholder */
        long start_output_offset = *output_offset_p;

        /* write bit packed table */
#if SHOW_TABLE
        printf("backreference displacement length table:\n");
#endif
        uint32_t bit_pool = 0;
        int bits_written = 8;   /* leave space for size */
        for (int i=1; i < table_size; i++)
        {
#if SHOW_TABLE
            printf("%d: %d\n", i, (int)tree_table[i]);
#endif
            WRITE_BITS(tree_table[i], DISPBITS);
        }
        flush_bits(outfile, output_offset_p, &bit_pool, &bits_written);

        long table_bytes = (*output_offset_p - start_output_offset) / 4 - 1 ;

        CHECK_ERROR( UINT8_MAX <= table_bytes, "displen table too big" );
        put_byte_seek(table_bytes, start_output_offset, outfile);

#if SHOW_TABLE
        printf("done at 0x%lx\n\n", *output_offset_p);
#endif
    }

}

/* Build tree for Huffman coding based on symbol frequencies. */
int LZH8_Huff_build_Huffman_tree(
    int *node_remains,
    long *freq,
    struct huff_node *node_array,
    int symbol_count
    )
{
    int nodes_left = 0;
    int next_new_node_idx = symbol_count;
    for (int i=0; i < symbol_count; i++)
    {
        if (0 != freq[i])
        {
            node_remains[i] = 1;
            nodes_left ++;
        }
        else
        {
            node_remains[i] = 0;
        }
        node_array[i].lchild = -1;
        node_array[i].rchild = -1;
        node_array[i].leaf = i;
        node_array[i].subtree_size = 0;
    }
    for (int i=symbol_count; i < symbol_count*2-1; i++)
    {
        node_remains[i] = 0;
    }

    int root_idx = 0;

    if ( 0 == nodes_left )
    {
        /* Cheat for zero nodes, return bad root_idx to avoid doing anything
           else with this tree. */

        return -1;
    }

    if ( 1 == nodes_left )
    {
        /* Cheat for one node */

        /* Find only symbol */
        int i;
        for (i = 0; i < symbol_count; i++)
        {
            if (node_remains[i])
            {
                break;
            }
        }

        /* root points to it twice */
        node_array[next_new_node_idx].lchild = i;
        node_array[next_new_node_idx].rchild = i;
        node_array[next_new_node_idx].subtree_size = 1;

        root_idx = next_new_node_idx;
    }

    for (; nodes_left > 1; nodes_left --)
    {
        /* find smallest two (POLICY: favor infrequency, then low index) */
        int smallest_idx = -1, next_smallest_idx = -1;
        {
            long smallest = -1, next_smallest = -1;
            for (int i=0; i < next_new_node_idx; i++)
            {
                if (node_remains[i])
                {
                    if (freq[i] < smallest || -1 == smallest)
                    {
                        next_smallest = smallest;
                        next_smallest_idx = smallest_idx;
                        smallest = freq[i];
                        smallest_idx = i;
                    }
                    else if (freq[i] < next_smallest || -1 == next_smallest)
                    {
                        next_smallest = freq[i];
                        next_smallest_idx = i;
                    }
                }
            }
        }

        /* construct new node to join the two */
        /* POLICY: smallest on left */
        struct huff_node sum_node;
        sum_node.lchild = smallest_idx;
        sum_node.rchild = next_smallest_idx;
        sum_node.leaf = 0;
        sum_node.subtree_size =
            node_array[smallest_idx].subtree_size + 
            node_array[next_smallest_idx].subtree_size + 1;

        /* new node has combined frequency of children */
        long total_freq = freq[smallest_idx] + freq[next_smallest_idx];

        /* POLICY: new node goes to end of queue */
        int sum_node_idx = next_new_node_idx;
        freq[sum_node_idx] = total_freq;
        node_remains[sum_node_idx] = 1;
        node_remains[smallest_idx] = 0;
        node_remains[next_smallest_idx] = 0;

        node_array[sum_node_idx] = sum_node;

        root_idx = sum_node_idx;

        next_new_node_idx ++;
    }

#if SHOW_TREE
    for (int i = next_new_node_idx-1; i >= 0; i--)
    {
        printf("node %d: ", i);
        if (-1 == node_array[i].lchild)
        {
            printf("leaf (%x)\n", (unsigned)node_array[i].leaf);
        }
        else
        {
            printf("lchild %d rchild %d\n",
                    node_array[i].lchild,
                    node_array[i].rchild);
        }
    }
    printf("\n");
#endif

    return root_idx;
}

/* Build the prefix codes for everything under root_idx, assuming
   key_bits (low key_len bits) has prefix so far. */
void LZH8_Huff_compute_prefix(
        const struct huff_node *node_array,
        int root_idx,
        struct huff_symbol *sym_array,
        uint32_t key_bits,
        int key_len)
{
    const struct huff_node *root = &node_array[root_idx];

    if ( -1 == root_idx )
    {
        /* no tree */
        return;
    }

    CHECK_ERROR(32 <= key_len, "key too long");
    CHECK_ERROR(
            (-1 == root->lchild && -1 != root->rchild) ||
            (-1 != root->lchild && -1 == root->rchild),
            "node not inner with two children or leaf");
    if (-1 != root->lchild)
    {
        key_len ++;
        LZH8_Huff_compute_prefix(node_array, root->lchild, sym_array,
                key_bits<<1, key_len);
        LZH8_Huff_compute_prefix(node_array, root->rchild, sym_array,
                (key_bits<<1)|1, key_len);
    }
    else
    {
        sym_array[root->leaf].key_len = key_len;
        sym_array[root->leaf].key_bits = key_bits;
#if SHOW_TREE_BITS
        printf("%d: ", root->leaf);
        for (int i=key_len-1; i>=0; i--)
        {
            printf("%c", (key_bits&(1<<i)) ? '1' : '0');
        }
        printf("\n");
#endif
    }
}

/* Generate the flat table for decoding. */
int LZH8_Huff_flatten_tree(
    const struct huff_node *node_array,
    uint16_t *tree_table,
    int root_idx,
    const int offset_bits)
{
    if ( -1 == root_idx )
    {
        /* no tree */
        return 0;
    }

    /* root_idx is leaf count + inner node count - 1, we need a maximum of
       leaf count + inner node count + 1 (extra to align pairs to even idx) */
    struct huff_table_ctrl *ctrl =
        malloc(sizeof(struct huff_table_ctrl) * (root_idx+2));
    CHECK_ERRNO(NULL == ctrl, "malloc");

    /* known unplaced nodes */
    unsigned int outstanding_nodes = 0;
    /* where the next entry goes */
    unsigned int table_idx;

    /* place root node */
    {
        outstanding_nodes = 1;
        ctrl[0].placed = true;

        ctrl[1].node_idx = root_idx;
        ctrl[1].placed = false;

        table_idx = 2;
    }

    while (0 < outstanding_nodes)
    {
#if EXPLAIN_TABLE
        printf("New round, table_idx = %d, outstanding_nodes = %d\n",
                table_idx, outstanding_nodes);
#endif
        uint16_t fitting_subtree_size = 0;
        uint16_t fitting_subtree_idx = table_idx;

        /* try to find a subtree that will fit, starting with most recent */
        for (int i = table_idx-1; i >= 0; i--)
        {
            if ( !ctrl[i].placed )
            {
                const struct huff_node *candidate =
                    &node_array[ctrl[i].node_idx];

#if EXPLAIN_TABLE
                printf(" considering %d, size %d (limit %d)\n", i,
                        candidate->subtree_size,
                        (1 << offset_bits) - outstanding_nodes);
#endif

                /* we'd like to place the whole subtree this points at */
                if ( candidate->subtree_size + outstanding_nodes <=
                        (1 << offset_bits) &&
                     LZH8_Huff_could_satisfy_outstanding_nodes(
                        ctrl, table_idx, candidate->subtree_size, i,
                        offset_bits )
                    )
                {
                    /* We can safely place this subtree (see note at
                       could_satisfy_outstanding_nodes) */

                    /* POLICY: use most recent (favoring right children) */
                    fitting_subtree_size = candidate->subtree_size;
                    fitting_subtree_idx = i;
                    break;
                }
            }
        }

        if ( fitting_subtree_idx != table_idx )
        {
            /* We found a subtree that wasn't too large,
               insert it at the end of the table */
#if EXPLAIN_TABLE
            printf(" found fitting subtree, %d, size %d\n",
                    fitting_subtree_idx, fitting_subtree_size );
#endif

            /* POLICY: breadth first traversal, left child first */
            /* (other traversals would work as well; we know the tree is small
               enough to be entirely skipped by a single offset) */

            /* we want to start the traversal on the new nodes added by placing
               the children of the root, so set the loop index to where the
               first (if any) will be placed */
            unsigned int i = table_idx;

            /* place the root */
            LZH8_Huff_flatten_single_node(
                node_array, ctrl, tree_table, offset_bits, fitting_subtree_idx,
                &table_idx, &outstanding_nodes);

            /* Consider any unplaced children. table_idx will continue to
               increase as the table is expanded with descendants of our
               original root. */
            for ( ; i < table_idx; i++ )
            {
                if ( !ctrl[i].placed )
                {
                    LZH8_Huff_flatten_single_node(
                        node_array, ctrl, tree_table, offset_bits, i,
                        &table_idx, &outstanding_nodes);
                }
            }
        }
        else
        {
            /* Not able to fit a whole subtree at this time. */
#if 0
            CHECK_ERROR(!  could_satisfy_outstanding_nodes(
                        ctrl, table_idx, 0, -1, offset_bits ),
                        "attempting to add impossible!");
#endif
            
            /* POLICY: Break up one outstanding subtree which is too large
                to deal with at once (any of them). Prefer the farthest, as it
                is the nearest to being unable to store. */

            for ( unsigned int i = 0; i < table_idx; i+=2 )
            {
                unsigned int node_to_break = table_idx;

                /* POLICY: If there are siblings (which are automatically at
                the same distance), choose the one with the largest subtree,
                favoring the left child in a tie. */
                if ( !ctrl[i+0].placed )
                {
                    if ( !ctrl[i+1].placed &&
                         node_array[ctrl[i+1].node_idx].subtree_size >
                         node_array[ctrl[i+0].node_idx].subtree_size )
                    {
                        node_to_break = i+1;
                    }
                    else
                    {
                        node_to_break = i+0;
                    }
                }
                else if ( !ctrl[i+1].placed )
                {
                    node_to_break = i+1;
                }

                if (node_to_break != table_idx)
                {
#if EXPLAIN_TABLE
                    printf(" couldn't fit subtree, storing %d\n",
                            node_to_break);
#endif
                    LZH8_Huff_flatten_single_node(
                        node_array, ctrl, tree_table, offset_bits,
                        node_to_break, &table_idx, &outstanding_nodes);
                    break;
                }
            }
        }
    }   /* end while 0 < outstanding_nodes */

    free(ctrl);

    return table_idx;
}

/* Place immediate children of a node into the tree table */
/* Node must not be a leaf. */
void LZH8_Huff_flatten_single_node(
    const struct huff_node *node_array,
    struct huff_table_ctrl *ctrl,
    uint16_t *tree_table,
    const int offset_bits,
    unsigned int parent_idx,          /* whose children we're placing */
    unsigned int *table_idx_p,        /* where to place (updated) */
    unsigned int *outstanding_nodes_p)/* count of unplaced nodes (updated) */
{
    uint8_t leaf_flags = 0;
    const struct huff_node *parent_node =
        &node_array[ctrl[parent_idx].node_idx];

    CHECK_ERROR( ctrl[parent_idx].placed, "trying to re-locate" );
    CHECK_ERROR( 0 != (*table_idx_p) % 2, "uneven table index");

    if (node_array[parent_node->lchild].lchild != -1)
    {
        /* left node is not a leaf */
        tree_table[*table_idx_p] = 0;
        ctrl[*table_idx_p].placed = false;
        ctrl[*table_idx_p].node_idx = parent_node->lchild;
        (*outstanding_nodes_p) ++;
    }
    else
    {
        /* left node is a leaf */
        tree_table[*table_idx_p] = node_array[parent_node->lchild].leaf;
        ctrl[*table_idx_p].placed = true;
        leaf_flags |= 2;    /* high bit, left node is leaf */
    }
    (*table_idx_p) ++;

    if (node_array[parent_node->rchild].lchild != -1)
    {
        /* right node is not a leaf */
        tree_table[*table_idx_p] = 0;
        ctrl[*table_idx_p].placed = false;
        ctrl[*table_idx_p].node_idx = parent_node->rchild;
        (*outstanding_nodes_p) ++;
    }
    else
    {
        /* right node is a leaf */
        tree_table[*table_idx_p] = node_array[parent_node->rchild].leaf;
        ctrl[*table_idx_p].placed = true;
        leaf_flags |= 1;    /* low bit, right node is leaf */
    }
    (*table_idx_p) ++;

    /* set link from parent table entry*/
    uint16_t offset = (((*table_idx_p) - 2) - parent_idx / 2 * 2) / 2 - 1;
    CHECK_ERROR( (1 << offset_bits) <= offset, "offset too large" );
    tree_table[parent_idx] = (leaf_flags << offset_bits) | offset;

    ctrl[parent_idx].placed = true;

    (*outstanding_nodes_p) --;
}

/* Simulation to find: 
    Given the current end of the table, if we were to store a table of
    proposed_size entries, would it still be possible to store offsets for
    the currently outstanding (unplaced) entries?

   Note: The size of a subtree includes its root node, which will also satisfy
   one of the outstanding entries. However, also note that the max_offset is
   counted from the beginning of the entry pair, it can only actually skip one
   fewer nodes than the offset.

   While this will be overcautious if there is only a single outstanding entry,
   that pointing at the proposed subtree, it is otherwise needed to ensure
   that the subtree can be skipped. Also in the single outstanding entry
   case, it will be the only check on the size of the subtree (which could
   potentially be too large to store in one go), though it remains somewhat
   overcautious.

   The check before can_satisfy_single_nodes up in LZH8_huff_flatten_tree is a
   shortcut to avoid calling this function if it would be impossible to satisfy
   the outstanding nodes even in the most generous case: the only outstanding
   node (thus the one placed first) is the last in the table (thus having the
   shortest possible offset required to jump the proposed subtree).
*/
bool LZH8_Huff_could_satisfy_outstanding_nodes(
    const struct huff_table_ctrl *ctrl,
    int table_idx,
    uint16_t proposed_size,
    int proposed_idx,
    const int offset_bits)
{
    /* jump is 2^offset_bits-1, +1 as it is assumed to proceed at least to the
       next entry pair */
    const int max_offset = (1 << offset_bits);

    CHECK_ERROR( 0 != table_idx % 2, "odd table index" );

    /* start from the end of the table (this is where we would start when
       actually adding the nodes) */
    for (unsigned int i = 0; i < table_idx; i++)
    {
        if (!ctrl[i].placed)
        {
            uint16_t dest_offset = table_idx/2 + proposed_size;

#if EXPLAIN_TABLE
            printf(" -simulate adding %d, table end = %d, "
                   "distance would be %d (limit %d)\n",
                   i, dest_offset, dest_offset - i / 2, max_offset);
#endif
            if (max_offset >= dest_offset - i / 2)
            {
                /* place that entry at the end, another one to jump over */
                proposed_size ++;
            }
            else
            {
#if EXPLAIN_TABLE
                printf(" *failed\n");
#endif

                /* cannot be placed */
                return false;
            }
        }
    }

    return true;
}
