#include <stdio.h>
#include <stdbool.h>

#include "xma_rebuild.h"
#include "util.h"
#include "error_stuff.h"
#include "bitstream.h"

// rebuild XMA2 streams as XMA
// based on xma_parse 0.12

struct xma_build_context
{
    unsigned int bits_written;  // bits written this packet
    unsigned int seqno;         // sequence number
};

struct xma_packet_header
{
    unsigned sequence_number: 4;
    unsigned unknown        : 2;
    unsigned skip_bits      : 15;
    unsigned packet_skip    : 11;
};

struct xma2_packet_header
{
    unsigned frame_count    : 6;
    unsigned skip_bits      : 15;
    unsigned metadata       : 3;
    unsigned packet_skip    : 8;
};

static void write_XMA_packet_header(struct bitstream_writer *obs, const struct xma_packet_header *h);
static long build_XMA_from_XMA2_block(const uint8_t *indata, struct bitstream_writer *obs, long offset, long block_size, struct xma_build_context *ctx, bool stereo, bool strict, bool last, bool verbose);
static long parse_frames(struct bitstream_reader *ibs, unsigned int frame_count, bool known_frame_count, unsigned int * total_bits_p, unsigned int max_bits, bool stereo, bool strict, bool verbose);
static int packetize(struct bitstream_reader *ibs, struct bitstream_writer *obs, struct xma_build_context * ctx, unsigned int frame_count, bool strict, bool last);

uint8_t *make_xma_header(uint32_t srate, uint32_t size, int channels)
{
    uint8_t *h = malloc(xma_header_size);
    CHECK_ERRNO(!h, "malloc");

    // RIFF header
    memcpy(&h[0x00], "RIFF", 4);
    write_32_le(size+0x3c-8, &h[0x04]); // RIFF size
    memcpy(&h[0x08], "WAVE", 4);

    // fmt chunk
    memcpy(&h[0x0C], "fmt ", 4);
    write_32_le(0x20, &h[0x10]);    // fmt chunk size

    write_16_le(0x165, &h[0x14]);   // WAVE_FORMAT_XMA
    write_16_le(16, &h[0x16]);      // 16 bits per sample
    write_16_le(0, &h[0x18]);       // encode options **
    write_16_le(0, &h[0x1a]);       // largest skip
    write_16_le(1, &h[0x1c]);       // # streams
    h[0x1e] = 0;    // loops
    h[0x1f] = 3;    // encoder version

    // lone stream info
    write_32_le(0, &h[0x20]);       // bytes per second **
    write_32_le(srate, &h[0x24]);   // sample rate

    write_32_le(0, &h[0x28]);       // loop start
    write_32_le(0, &h[0x2c]);       // loop end
    h[0x30] = 0;    // subframe loop data

    h[0x31] = channels;             // channels
    write_16_le(0x0002, &h[0x32]);  // channel mask

    // data chunk
    memcpy(&h[0x34], "data", 4);
    write_32_le(size, &h[0x38]);    // data chunk size

    return h;
}

int build_XMA_from_XMA2(const uint8_t *indata, long data_size, FILE *outfile, long block_size, int channels, long *samples_p)
{
    long total_sample_count = 0;

    struct xma_build_context ctx;
    struct bitstream_writer *obs;

    // initialize
    obs = init_bitstream_writer(outfile);
    {
        struct xma_packet_header h = {
            .sequence_number = 0,
            .unknown = 2,
            .skip_bits = 0,
            .packet_skip = 0};
        write_XMA_packet_header(obs, &h);
    }
    ctx.bits_written = packet_header_size_bytes * 8;
    ctx.seqno = 1;

    // handle blocks
    for (long block_offset = 0;
        block_offset < data_size;
        block_offset += block_size) {

        long sample_count = 0;
        long usable_block_size = block_size;
        bool strict = true, verbose = false;

        if (block_offset + usable_block_size > data_size)
        {
            usable_block_size = data_size - block_offset;
        }

        sample_count = build_XMA_from_XMA2_block(indata, obs, block_offset, usable_block_size, &ctx, (channels > 1),
            strict, (block_offset + block_size >= data_size), verbose);

        if (-1 == sample_count)
        {
            return 1;
        }

        total_sample_count += sample_count;

        if (verbose) {
            printf("\n%ld samples (block) (%ld total)\n\n", sample_count, total_sample_count);
        }
    }

    // finish
    // pad with ones
    for (; ctx.bits_written < packet_size_bytes * 8; ctx.bits_written ++) {
        put_bit(obs, 1);
    }
    flush_bitstream_writer(obs);
    free_bitstream_writer(obs);

    if (samples_p)
    {
        *samples_p = total_sample_count;
    }

    return 0;
}

static void write_XMA_packet_header(struct bitstream_writer *obs, const struct xma_packet_header *h)
{
    put_bits(obs, h->sequence_number, 4);
    put_bits(obs, h->unknown, 2);
    put_bits(obs, h->skip_bits, 15);
    put_bits(obs, h->packet_skip, 11);
}

static void read_XMA2_packet_header(struct bitstream_reader *ibs, struct xma2_packet_header *h)
{
    h->frame_count = get_bits(ibs, 6);
    h->skip_bits = get_bits(ibs, 15);
    h->metadata = get_bits(ibs, 3);
    h->packet_skip = get_bits(ibs, 8);
}

static long build_XMA_from_XMA2_block(const uint8_t *indata, struct bitstream_writer *obs, long offset, long block_size, struct xma_build_context *ctx, bool stereo, bool strict, bool last, bool verbose)
{
    long last_offset = offset + block_size;
    unsigned int sample_count = 0;
    unsigned int last_packet_overflow_bits = 0;

    for (unsigned packet_number = 0; offset < last_offset; packet_number++) {
        struct xma2_packet_header ph;

        {
            struct bitstream_reader *ibs = init_bitstream_reader(indata+offset, packet_header_size_bytes, 0, 0);
            read_XMA2_packet_header(ibs, &ph);
            free_bitstream_reader(ibs);
        }

        if (verbose) {
            printf("Packet #%u (offset 0x%lx)\n", packet_number, (unsigned long)offset);
            printf("Frame Count     %u\n", ph.frame_count);
            printf("Skip Bits       %u\n", ph.skip_bits);
            printf("Metadata        %u\n", ph.metadata);
            printf("Packet Skip     %u\n", ph.packet_skip);
        }

#if 0
        if (ignore_packet_skip)
        {
            ph.packet_skip = 0;
        }
#endif

        struct bitstream_reader *ibs = init_bitstream_reader(
            // where's the data
            indata + offset + packet_header_size_bytes,
            // should never reach past end of block
            last_offset - (offset + packet_header_size_bytes),
            // consecutive
            (packet_size_bytes - packet_header_size_bytes) * 8,
            // skip
            (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
        );

        // At the end of a block no frame may start in a packet, signalled
        // with invalidly large skip_bits so we skip everything
        if (ph.skip_bits == 0x7fff) {
            if (ph.frame_count != 0) {
                //throw Skip_nonzero_frames();
                free_bitstream_reader(ibs);
                return -1;
            }
            last_packet_overflow_bits = 0;
        } else {
            unsigned int total_bits;
            long packet_sample_count;

            // skip initial bits (overflow from a previous packet)
            for (unsigned int i = 0; i < ph.skip_bits; i++) get_bit(ibs);

            if (ph.skip_bits != last_packet_overflow_bits) {
                //throw Skip_mismatch(ph.skip_bits,last_packet_overflow_bits);
                free_bitstream_reader(ibs);
                return -1;
            }

            packet_sample_count = parse_frames(ibs, ph.frame_count, true, &total_bits, (packet_size_bytes - packet_header_size_bytes)*8 - ph.skip_bits, stereo, strict, verbose);
            if (-1 == packet_sample_count)
            {
                free_bitstream_reader(ibs);
                return -1;
            }
            sample_count += packet_sample_count;

            int overflow_temp = last_packet_overflow_bits = (ph.skip_bits + total_bits) - ((packet_size_bytes - packet_header_size_bytes) * 8);
            if (overflow_temp > 0) {
                last_packet_overflow_bits = overflow_temp;
            } else {
                last_packet_overflow_bits = 0;
            }
        }

        free_bitstream_reader(ibs);

        // We've successfully examined this packet, dump it out
        {
            struct bitstream_reader *dump_ibs = init_bitstream_reader(
                // where's the data
                indata + offset + packet_header_size_bytes,
                // should never reach past end of block
                last_offset - (offset + packet_header_size_bytes),
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
            );

            // Do packet if not skipping
            if (ph.skip_bits != 0x7fff) {
                // skip initial bits (overflow from a previous packet)
                for (unsigned int i = 0; i < ph.skip_bits; i++) get_bit(dump_ibs);

                if (0 != packetize(dump_ibs, obs, ctx, ph.frame_count, strict,
                        last && ((unsigned long)offset + (ph.packet_skip + 1) * packet_size_bytes >= (unsigned long)last_offset) ))
                {
                    free_bitstream_reader(dump_ibs);
                    return -1;
                }
            }

            free_bitstream_reader(dump_ibs);
        }

        // advance to next packet
        offset += (ph.packet_skip + 1) * packet_size_bytes;
    }

    return sample_count;
}

static long parse_frames(struct bitstream_reader *ibs, unsigned int frame_count, bool known_frame_count, unsigned int * total_bits_p, unsigned int max_bits, bool stereo, bool strict, bool verbose) {
    bool packet_end_seen = false;
    unsigned int sample_count = 0;
    unsigned int total_bits = 0;

    if (known_frame_count && frame_count == 0) {
        //throw Zero_frames_not_skipped();
        return -1;
    }

    for (unsigned int frame_number = 0;
         !known_frame_count || frame_number < frame_count;
         frame_number++)
    {

        unsigned int frame_bits = get_bits(ibs, frame_header_size_bits);
        total_bits += frame_bits;

        unsigned int bits_left = frame_bits - frame_header_size_bits;

        if (verbose)
        {
            printf("   Frame #%u\n", frame_number);
            printf("   Size %u\n", frame_bits);
        }

        // sync
        {
            unsigned int sync = get_bits(ibs, frame_sync_size_bits);
            if (sync!= 0x7f00)
            {
                //throw Bad_frame_sync(sync);
                return -1;
            }
            bits_left -= frame_sync_size_bits;
        }

#if 0
        if (stereo) {
            get_bit(ibs);
            bits_left --;
        }
#endif

#if 0
        // skip
        if (get_bit(ibs)) {
#if 0
            // skip at start
            if (get_bit(ibs)) {
                unsigned int skip_start = get_bits(ibs, frame_skip_size_bits);
                if (verbose) {
                    printf("Skip %u samples at start\n", skip_start);
                }
                bits_left -= frame_skip_size_bits;
                sample_count -= skip_start;
            }
            bits_left --;
#endif
#if 0
            // skip at end
            if (get_bit(ibs)) {
                unsigned int skip_end = get_bits(ibs, frame_skip_size_bits);
                if (verbose) {
                    printf("Skip %u samples at end\n", skip_end);
                }
                bits_left -= frame_skip_size_bits;
                sample_count -= skip_end;
            }
            bits_left --;
#endif
        }
        bits_left --;
#endif

        for (; bits_left >= 4 + frame_trailer_size_bits; bits_left -= 4) {
            unsigned int nybble = get_bits(ibs, 4);
            if (verbose) {
                printf("%1x\n", nybble);
            }
        }
        if (verbose) {
            printf(" ");
        }
        for (; bits_left > frame_trailer_size_bits; bits_left--) {
            unsigned int bit = get_bit(ibs);
            if (verbose) {
                printf("%c", (bit ? '1' : '0'));
            }
        }

        // trailer
        {
            if (!get_bit(ibs))
            {
                if (strict && known_frame_count &&
                    frame_number != frame_count-1)
                {
                    //throw Early_packet_end();
                    return -1;
                }
                packet_end_seen = true;
            }

            sample_count += samples_per_frame;

            bits_left -= frame_trailer_size_bits;

            if (!known_frame_count && packet_end_seen) break;

            // FIX: detect end with bit count
            if (!strict && !known_frame_count && total_bits >= max_bits) {
                if (verbose) {
                    printf("abandon frame due to bit count (total=%u max=%u)\n", total_bits, max_bits);
                }
                break;
            }
        }
    }

    // FIX: don't fail if packet end missing
    if (strict && !packet_end_seen)
    {
        //throw Missing_packet_end();
        return -1;
    }

    if (verbose) {
        printf("\n");
    }

    if (total_bits_p)
    {
        *total_bits_p = total_bits;
    }

    return sample_count;
}

static int packetize(struct bitstream_reader *ibs, struct bitstream_writer *obs, struct xma_build_context * ctx, unsigned int frame_count, bool strict, bool last) {
    bool packet_end_seen = false;
    unsigned int bits_written = ctx->bits_written;
    unsigned int seqno = ctx->seqno;

    if (frame_count == 0) {
        //throw Zero_frames_not_skipped();
        return -1;
    }

    for (unsigned int frame_number = 0;
         frame_number < frame_count;
         frame_number++) {
        unsigned int frame_bits = get_bits(ibs, frame_header_size_bits);

        if (bits_written + frame_bits >= packet_size_bytes * 8) {
            unsigned int bits_this_packet = (packet_size_bytes * 8) - bits_written;
            unsigned int overflow_bits = frame_bits - bits_this_packet;

            struct xma_packet_header ph;
            ph.sequence_number = seqno;
            seqno = (seqno + 1) % 16;
            ph.unknown = 2;
            ph.skip_bits = overflow_bits;
            ph.packet_skip = 0;

            // bits of frame header before packet end
            unsigned int frame_header_size_bits_left = frame_header_size_bits;
            for (; bits_this_packet > 0 && frame_header_size_bits_left > 0; bits_this_packet --, frame_header_size_bits_left --) {
                put_bit(obs, 0 != (frame_bits & (1 << (frame_header_size_bits_left - 1))) );
            }

            if (overflow_bits == 0) {
                // frame fits packet exactly

                // payload bits before packet end
                for (unsigned int i = 0; i < bits_this_packet-1; i++) {
                    put_bit(obs, get_bit(ibs));
                }
                // trailer bit, no more frames in packet
                put_bit(obs, 0);
            } else {
                // payload bits 
                for (unsigned int i = 0; i < bits_this_packet; i++) {
                    put_bit(obs, get_bit(ibs));
                }
            }

            write_XMA_packet_header(obs, &ph);

            bits_written = packet_header_size_bytes * 8;

            if (overflow_bits != 0) {
                // bits of frame header in new packet
                for (; frame_header_size_bits_left > 0; frame_header_size_bits_left --, bits_written ++, overflow_bits --) {
                    put_bit(obs, 0 != (frame_bits & (1 << (frame_header_size_bits_left - 1))) );
                }

                // payload bits in new packet
                for (unsigned int i = 0; i < overflow_bits - 1; i++) {
                    put_bit(obs, get_bit(ibs));
                }
                bits_written += overflow_bits - 1;

                // trailer bit, no more frames in packet
                put_bit(obs, 0);
                bits_written ++;
            }
        } else {
            put_bits(obs, frame_bits, frame_header_size_bits);
            for (unsigned int i = 0; i < frame_bits - frame_header_size_bits - 1; i++) {
                put_bit(obs, get_bit(ibs));
            }

            // trailer bit
            if (last && frame_number == frame_count-1) {
                // no more frames
                put_bit(obs, 0);
            } else {
                // more frames in packet
                put_bit(obs, 1);
            }

            bits_written += frame_bits;
        }

        // trailer
        {
            if (!get_bit(ibs))
            {
                if (strict && frame_number != frame_count-1)
                {
                    //throw Early_packet_end();
                    return -1;
                }
                packet_end_seen = true;
            }
        }
    }

    if (strict && !packet_end_seen)
    {
        //throw Missing_packet_end();
        return -1;
    }

    ctx->seqno = seqno;
    ctx->bits_written = bits_written;

    return 0;
}
