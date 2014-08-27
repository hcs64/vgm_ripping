#include <iostream>
#include "Bit_stream.h"
#include "xma_parse.h"

using namespace std;

unsigned int Parse_XMA::parse_XMA_packets(istream& is, ostream& os, long offset, long data_size, bool stereo, bool strict, bool verbose, bool ignore_packet_skip) {
    long last_offset = offset + data_size;
    unsigned long sample_count = 0;
    unsigned int last_packet_overflow_bits = 0;
    unsigned int seqno = 0;

    while (offset < last_offset) {
        xma_packet_header ph;

        {
            Bit_stream packet_header_stream(is);

            is.seekg(offset);

            packet_header_stream >> ph;
        }

        if (verbose) {
            cout << "Sequence #" << ph.sequence_number << " (offset " << hex << offset << dec << ")" << endl;
            cout << "Unknown         " << ph.unk << endl;
            cout << "Skip Bits       " << ph.skip_bits << endl;
            cout << "Packet Skip     " << ph.packet_skip << (ignore_packet_skip?" (ignore)":"") << endl;
        }

        if (ignore_packet_skip) {
            ph.packet_skip = 0;
        }

        if (strict && ph.sequence_number != seqno) {
            throw Bad_sequence();
        }

        Bit_stream frame_stream(is,
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
                );

        if (16384 == ph.skip_bits)
        {
            if (ph.unk != 0) {
                cout << "Unknown = " << ph.unk << ", expected 0" << endl;
            }

            last_packet_overflow_bits = 0;
        }
        else
        {
            unsigned int total_bits;

            if (ph.unk != 2) {
                cout << "Unknown = " << ph.unk << ", expected 2" << endl;
            }

            // skip initial bits (overflow from a previous packet)
            for (unsigned int i = 0; i < ph.skip_bits; i++) frame_stream.get_bit();

            if (ph.skip_bits != last_packet_overflow_bits)
            {
                throw Skip_mismatch(ph.skip_bits,last_packet_overflow_bits);
            }

            sample_count += parse_frames(frame_stream, 0, false, &total_bits, (packet_size_bytes - packet_header_size_bytes)*8 - ph.skip_bits, stereo, strict, verbose);

            int overflow_temp = last_packet_overflow_bits = (ph.skip_bits + total_bits) - ((packet_size_bytes - packet_header_size_bytes) * 8);
            if (overflow_temp > 0) {
                last_packet_overflow_bits = overflow_temp;
            } else {
                last_packet_overflow_bits = 0;
            }
        }

        // We've successfully examined this packet, dump it out if we have an output stream
        if (os) {
            char buf[packet_size_bytes];
            is.seekg(offset);

            is.read(buf, packet_size_bytes);
            // FIX: fix sequence number
            if (seqno != ph.sequence_number && !strict) {
                buf[0] = (buf[0] & 0xf) | (seqno << 4);
                if (verbose) {
                    cout << "fixing sequence number (was " << ph.sequence_number << ", output " << seqno << ")" << endl;
                }
            }
            buf[3] = 0; // zero packet skip, since we're packing consecutively
            os.write(buf, packet_size_bytes);
        }

        seqno = (seqno + 1) % 16;

        offset += (ph.packet_skip + 1) * packet_size_bytes;
    }

    return sample_count;
}

unsigned int Parse_XMA::parse_XMA2_block(istream& is, ostream& os, long offset, long block_size, bool stereo, bool strict, bool verbose, bool ignore_packet_skip) {
    long last_offset = offset + block_size;
    unsigned long sample_count = 0;
    unsigned int last_packet_overflow_bits = 0;

    for (int packet_number = 0; offset < last_offset; packet_number++) {
        xma2_packet_header ph;

        {
            Bit_stream packet_header_stream(is);

            is.seekg(offset);

            packet_header_stream >> ph;
        }

        if (verbose) {
            cout << "Packet #" << packet_number << " (offset " << hex << offset << dec << ")" << endl;
            cout << "Frame Count     " << ph.frame_count << endl;
            cout << "Skip Bits       " << ph.skip_bits << endl;
            cout << "Metadata        " << ph.metadata << endl;
            cout << "Packet Skip     " << ph.packet_skip << (ignore_packet_skip?" (ignored)":"") << endl;
        }

        if (ignore_packet_skip)
        {
            ph.packet_skip = 0;
        }

        Bit_stream frame_stream(is,
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
                );

        // At the end of a block no frame may start in a packet, signalled
        // with invalidly large skip_bits so we skip everything
        if (ph.skip_bits == 0x7fff) {
            if (ph.frame_count != 0) {
                throw Skip_nonzero_frames();
            }
            last_packet_overflow_bits = 0;
        } else {
            unsigned int total_bits;

            // skip initial bits (overflow from a previous packet)
            for (unsigned int i = 0; i < ph.skip_bits; i++) frame_stream.get_bit();

            if (ph.skip_bits != last_packet_overflow_bits) {
                throw Skip_mismatch(ph.skip_bits,last_packet_overflow_bits);
            }

            sample_count += parse_frames(frame_stream, ph.frame_count, true, &total_bits, (packet_size_bytes - packet_header_size_bytes)*8 - ph.skip_bits, stereo, strict, verbose);

            int overflow_temp = last_packet_overflow_bits = (ph.skip_bits + total_bits) - ((packet_size_bytes - packet_header_size_bytes) * 8);
            if (overflow_temp > 0) {
                last_packet_overflow_bits = overflow_temp;
            } else {
                last_packet_overflow_bits = 0;
            }
        }

        // We've successfully examined this packet, dump it out if we have an output stream
        if (os) {
            char buf[packet_size_bytes];
            is.seekg(offset);

            is.read(buf, packet_size_bytes);
            buf[3] = 0; // zero packet skip, since we're packing consecutively
            os.write(buf, packet_size_bytes);
        }

        // advance to next packet
        offset += (ph.packet_skip + 1) * packet_size_bytes;
    }

    return sample_count;
}

unsigned int Parse_XMA::parse_frames(Bit_stream& frame_stream, unsigned int frame_count, bool known_frame_count, unsigned int * total_bits_p, unsigned int max_bits, bool stereo, bool strict, bool verbose) {
    bool packet_end_seen = false;
    unsigned int sample_count = 0;
    unsigned int total_bits = 0;

    if (known_frame_count && frame_count == 0) {
        throw Zero_frames_not_skipped();
    }

    for (unsigned int frame_number = 0;
         !known_frame_count || frame_number < frame_count;
         frame_number++) {
        Bit_uint<frame_header_size_bits> frame_bits;
        frame_stream >> frame_bits;
        //cout << "   Frame #" << frame_number << ", " << total_bits << " bits read" << endl;
        total_bits += frame_bits;

        unsigned int bits_left = frame_bits - frame_header_size_bits;

        if (verbose)
        {
            cout << "   Frame #" << frame_number << endl;
            cout << "   Size " << frame_bits << endl;
        }

        // sync?
        {
            Bit_uint<frame_sync_size_bits> sync;
            frame_stream >> sync;
            if (sync!= 0x7f00) throw Bad_frame_sync(sync);
            bits_left -= frame_sync_size_bits;
        }

        if (stereo) {
            frame_stream.get_bit();
            bits_left --;
        }

        // skip
        if (frame_stream.get_bit()) {
            Bit_uint<frame_skip_size_bits> skip_start, skip_end;
            // skip at start
            if (frame_stream.get_bit()) {
                frame_stream >> skip_start;
                if (verbose) {
                    cout << "Skip " << skip_start << " samples at start" << endl;
                }
                bits_left -= frame_skip_size_bits;
                sample_count -= skip_start;
            }
            bits_left --;
            // skip at end
            if (frame_stream.get_bit()) {
                frame_stream >> skip_end;
                if (verbose) {
                    cout << "Skip " << skip_end << " samples at end" << endl;
                }
                bits_left -= frame_skip_size_bits;
                sample_count -= skip_end;
            }
            bits_left --;
        }
        bits_left --;

        if (verbose) {
            cout << hex;
        }
        for (; bits_left >= 4 + frame_trailer_size_bits; bits_left -= 4) {
            Bit_uint<4> nybble;
            frame_stream >> nybble;
            if (verbose) {
                cout << nybble;
            }
        }
        if (verbose) {
            cout << " ";
        }
        for (; bits_left > frame_trailer_size_bits; bits_left--) {
            bool bit = frame_stream.get_bit();
            if (verbose) {
                cout << (bit ? '1' : '0');
            }
        }
        if (verbose) {
            cout << dec << endl;
        }

        // trailer
        {
            if (!frame_stream.get_bit())
            {
                if (strict && known_frame_count &&
                    frame_number != frame_count-1) throw Early_packet_end();
                packet_end_seen = true;
            }

            sample_count += samples_per_frame;

            bits_left -= frame_trailer_size_bits;

            if (!known_frame_count && packet_end_seen) break;

            // FIX: detect end with bit count
            if (!strict && !known_frame_count && total_bits >= max_bits) {
                if (verbose) {
                    cout << "abandon frame due to bit count (total=" << total_bits << " max=" << max_bits << ")" << endl;
                }
                break;
            }
        }
    }

    // FIX: don't fail if packet end missing
    if (strict && !packet_end_seen) throw Missing_packet_end();

    if (verbose) {
        cout << endl;
    }

    if (total_bits_p)
    {
        *total_bits_p = total_bits;
    }

    return sample_count;
}

void Parse_XMA::init_build_XMA(Bit_ostream& bs, xma_build_context * ctx) {
    // first packet
    Parse_XMA::xma_packet_header ph;
    ph.sequence_number = 0;
    ph.unk = 2;
    ph.skip_bits = 0;
    ph.packet_skip = 0;

    bs << ph;

    ctx->bits_written = Parse_XMA::packet_header_size_bytes * 8;
    ctx->seqno = 1;
}

unsigned int Parse_XMA::build_XMA_from_XMA(istream& is, Bit_ostream& bs, long offset, long data_size, xma_build_context * ctx, bool stereo, bool strict, bool verbose, bool ignore_packet_skip) {
    long last_offset = offset + data_size;
    unsigned long sample_count = 0;
    unsigned int last_packet_overflow_bits = 0;
    unsigned int seqno = 0;

    while (offset < last_offset) {
        xma_packet_header ph;

        unsigned int frames_this_packet = 0;

        {
            Bit_stream packet_header_stream(is);

            is.seekg(offset);

            packet_header_stream >> ph;
        }

        if (verbose) {
            cout << "Sequence #" << ph.sequence_number << " (offset " << hex << offset << dec << ")" << endl;
            cout << "Unknown         " << ph.unk << endl;
            cout << "Skip Bits       " << ph.skip_bits << endl;
            cout << "Packet Skip     " << ph.packet_skip << (ignore_packet_skip?" (ignored)":"") << endl;
        }

        if (ignore_packet_skip)
        {
            ph.packet_skip = 0;
        }

        if (strict && ph.sequence_number != seqno) {
            throw Bad_sequence();
        }


        Bit_stream frame_stream(is,
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
                );

        if (16384 == ph.skip_bits)
        {
            last_packet_overflow_bits = 0;

            if (ph.unk != 0) {
                cout << "Unknown = " << ph.unk << ", expected 0" << endl;
            }
        }
        else
        {
            unsigned int total_bits;

            if (ph.unk != 2) {
                cout << "Unknown = " << ph.unk << ", expected 2" << endl;
            }

            // skip initial bits (overflow from a previous packet)
            for (unsigned int i = 0; i < ph.skip_bits; i++) frame_stream.get_bit();

            if (ph.skip_bits != last_packet_overflow_bits)
            {
                throw Skip_mismatch(ph.skip_bits,last_packet_overflow_bits);
            }

            unsigned int samples_this_packet = parse_frames(frame_stream, 0, false, &total_bits, (packet_size_bytes - packet_header_size_bytes)*8 - ph.skip_bits, stereo, strict, verbose);
            sample_count += samples_this_packet;
            frames_this_packet = samples_this_packet / samples_per_frame;

            int overflow_temp = last_packet_overflow_bits = (ph.skip_bits + total_bits) - ((packet_size_bytes - packet_header_size_bytes) * 8);
            if (overflow_temp > 0) {
                last_packet_overflow_bits = overflow_temp;
            } else {
                last_packet_overflow_bits = 0;
            }
        }

        // We've successfully examined this packet, dump it out
        {
            is.seekg(offset + packet_header_size_bytes);

            Bit_stream dump_frame_stream(is,
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
                );

            // Do packet if not skipping
            if (ph.skip_bits != 16384) {
                // skip initial bits (overflow from a previous packet)
                for (unsigned int i = 0; i < ph.skip_bits; i++) dump_frame_stream.get_bit();

                packetize(dump_frame_stream, bs, ctx, frames_this_packet, strict,
                        (static_cast<unsigned long>(offset) + (ph.packet_skip + 1) * packet_size_bytes >= static_cast<unsigned long>(last_offset)) );
            }
        }

        seqno = (seqno + 1) % 16;

        offset += (ph.packet_skip + 1) * packet_size_bytes;
    }

    return sample_count;
}


unsigned int Parse_XMA::build_XMA_from_XMA2_block(istream& is, Bit_ostream& bs, long offset, long block_size, xma_build_context * ctx, bool stereo, bool strict, bool last, bool verbose, bool ignore_packet_skip) {
    long last_offset = offset + block_size;
    unsigned int sample_count = 0;
    unsigned int last_packet_overflow_bits = 0;

    for (int packet_number = 0; offset < last_offset; packet_number++) {
        xma2_packet_header ph;

        {
            Bit_stream packet_header_stream(is);

            is.seekg(offset);

            packet_header_stream >> ph;
        }

        if (verbose) {
            cout << "Packet #" << packet_number << " (offset " << hex << offset << dec << ")" << endl;
            cout << "Frame Count     " << ph.frame_count << endl;
            cout << "Skip Bits       " << ph.skip_bits << endl;
            cout << "Metadata        " << ph.metadata << endl;
            cout << "Packet Skip     " << ph.packet_skip << (ignore_packet_skip?" (ignored)":"") << endl;
        }

        if (ignore_packet_skip)
        {
            ph.packet_skip = 0;
        }

        Bit_stream frame_stream(is,
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
                );

        // At the end of a block no frame may start in a packet, signalled
        // with invalidly large skip_bits so we skip everything
        if (ph.skip_bits == 0x7fff) {
            if (ph.frame_count != 0) {
                throw Skip_nonzero_frames();
            }
            last_packet_overflow_bits = 0;
        } else {
            unsigned int total_bits;

            // skip initial bits (overflow from a previous packet)
            for (unsigned int i = 0; i < ph.skip_bits; i++) frame_stream.get_bit();

            if (ph.skip_bits != last_packet_overflow_bits) {
                throw Skip_mismatch(ph.skip_bits,last_packet_overflow_bits);
            }

            sample_count += parse_frames(frame_stream, ph.frame_count, true, &total_bits, (packet_size_bytes - packet_header_size_bytes)*8 - ph.skip_bits, stereo, strict, verbose);

            int overflow_temp = last_packet_overflow_bits = (ph.skip_bits + total_bits) - ((packet_size_bytes - packet_header_size_bytes) * 8);
            if (overflow_temp > 0) {
                last_packet_overflow_bits = overflow_temp;
            } else {
                last_packet_overflow_bits = 0;
            }
        }

        // We've successfully examined this packet, dump it out
        {
            is.seekg(offset + packet_header_size_bytes);

            Bit_stream dump_frame_stream(is,
                // consecutive
                (packet_size_bytes - packet_header_size_bytes) * 8,
                // skip
                (packet_header_size_bytes + ph.packet_skip * packet_size_bytes) * 8
                );

            // Do packet if not skipping
            if (ph.skip_bits != 0x7fff) {
                // skip initial bits (overflow from a previous packet)
                for (unsigned int i = 0; i < ph.skip_bits; i++) dump_frame_stream.get_bit();

                packetize(dump_frame_stream, bs, ctx, ph.frame_count, strict,
                        last && (static_cast<unsigned long>(offset) + (ph.packet_skip + 1) * packet_size_bytes >= static_cast<unsigned long>(last_offset)) );
            }
        }

        // advance to next packet
        offset += (ph.packet_skip + 1) * packet_size_bytes;
    }

    return sample_count;
}

void Parse_XMA::packetize(Bit_stream& frame_stream, Bit_ostream& out_stream, xma_build_context * ctx, unsigned int frame_count, bool strict, bool last) {
    bool packet_end_seen = false;
    unsigned int bits_written = ctx->bits_written;
    unsigned int seqno = ctx->seqno;

    if (frame_count == 0) {
        throw Zero_frames_not_skipped();
    }

    for (unsigned int frame_number = 0;
         frame_number < frame_count;
         frame_number++) {
        Bit_uint<frame_header_size_bits> frame_bits;
        frame_stream >> frame_bits;

        //cout << "Frame #" << frame_number << ", " << bits_written << " bits written" << endl;

        if (bits_written + frame_bits >= packet_size_bytes * 8) {
            unsigned int bits_this_packet = (packet_size_bytes * 8) - bits_written;
            unsigned int overflow_bits = frame_bits - bits_this_packet;

            xma_packet_header ph;
            ph.sequence_number = seqno;
            seqno = (seqno + 1) % 16;
            ph.unk = 2;
            ph.skip_bits = overflow_bits;
            ph.packet_skip = 0;

            //cout << "overflow = " << overflow_bits << endl;

            // bits of frame header before packet end
            unsigned int frame_header_size_bits_left = frame_header_size_bits;
            for (; bits_this_packet > 0 && frame_header_size_bits_left > 0; bits_this_packet --, frame_header_size_bits_left --) {
                out_stream.put_bit( 0 != (frame_bits & (1 << (frame_header_size_bits_left - 1))) );
            }

            if (overflow_bits == 0) {
                // frame fits packet exactly

                // payload bits before packet end
                for (unsigned int i = 0; i < bits_this_packet-1; i++) {
                    out_stream.put_bit(frame_stream.get_bit());
                }
                // trailer bit, no more frames in packet
                out_stream.put_bit(false);
            } else {
                // payload bits 
                for (unsigned int i = 0; i < bits_this_packet; i++) {
                    out_stream.put_bit(frame_stream.get_bit());
                }
            }

            out_stream << ph;

            bits_written = packet_header_size_bytes * 8;

            if (overflow_bits != 0) {
                // bits of frame header in new packet
                for (; frame_header_size_bits_left > 0; frame_header_size_bits_left --, bits_written ++, overflow_bits --) {
                    out_stream.put_bit( 0 != (frame_bits & (1 << (frame_header_size_bits_left - 1))) );
                }

                // payload bits in new packet
                for (unsigned int i = 0; i < overflow_bits - 1; i++) {
                    out_stream.put_bit(frame_stream.get_bit());
                }
                bits_written += overflow_bits - 1;

                // trailer bit, no more frames in packet
                out_stream.put_bit(false);
                bits_written ++;
            }
        } else {
            out_stream << frame_bits;
            for (unsigned int i = 0; i < frame_bits - frame_header_size_bits - 1; i++) {
                out_stream.put_bit(frame_stream.get_bit());
            }

            // trailer bit
            if (last && frame_number == frame_count-1) {
                // no more frames
                out_stream.put_bit(false);
            } else {
                // more frames in packet
                out_stream.put_bit(true);
            }

            bits_written += frame_bits;
        }

        // trailer
        {
            if (!frame_stream.get_bit())
            {
                if (strict && frame_number != frame_count-1) throw Early_packet_end();
                packet_end_seen = true;
            }
        }
    }

    if (strict && !packet_end_seen) throw Missing_packet_end();

    ctx->seqno = seqno;
    ctx->bits_written = bits_written;
}

void Parse_XMA::finish_build_XMA(Bit_ostream& bs, xma_build_context * ctx) {
    // pad out with cool ones
    for (; ctx->bits_written < packet_size_bytes * 8; ctx->bits_written ++) {
        bs.put_bit(true);
    }
}
