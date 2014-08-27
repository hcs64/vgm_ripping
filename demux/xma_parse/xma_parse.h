#ifndef _XMA_PARSE_H
#define _XMA_PARSE_H

#include <iostream>

#include "Bit_stream.h"

namespace Parse_XMA {
    using namespace std;

    class Parse_error {
    public:
        virtual void print_self(ostream& os) const = 0;
        friend ostream& operator << (ostream& os, const Parse_error& pe) {
            os << "Parse error: ";
            pe.print_self(os);
            return os;
        }
        virtual ~Parse_error() {}
    };
    class Bad_frame_sync : public Parse_error {
        const unsigned int sync_val;
    public:
        explicit Bad_frame_sync(unsigned int v) : sync_val(v) {}
        void print_self(ostream& os) const {
            os << "unexpected \"frame sync\" " << hex << sync_val<< dec << endl;
        }
    };

    class Early_packet_end : public Parse_error {
    public:
        void print_self(ostream& os) const {
            os << "packet end indicated before end of packet" << endl;
        }
    };

    class Missing_packet_end : public Parse_error {
    public:
        void print_self(ostream& os) const {
            os << "packet end not seen before end of packet" << endl;
        }
    };

    class Zero_frames_not_skipped : public Parse_error {
    public:
        void print_self(ostream &os) const {
            os << "zero frames in this packet, but not set to skip it" << endl;
        }
    };

    class Skip_mismatch : public Parse_error {
        const unsigned int skip, overflow;
    public:
        Skip_mismatch(unsigned int s, unsigned int o) : skip(s), overflow(o) {}
        void print_self(ostream &os) const {
            os << "skip bits (" << skip << ") did not match previous packet overflow (" << overflow << ")" << endl;
        }
    };

    class Skip_nonzero_frames : public Parse_error {
    public:
        void print_self(ostream &os) const {
            os << "skipping entire packet with > 0 frames" << endl;
        }
    };

    class Bad_sequence : public Parse_error {
    public:
        void print_self(ostream &os) const {
            os << "packet out of sequence" << endl;
        }
    };

    enum {
        packet_size_bytes = 0x800,
        packet_header_size_bytes = 4,
        frame_header_size_bits = 15,
        frame_sync_size_bits = 15,
        frame_skip_size_bits = 10,
        frame_trailer_size_bits = 1,
        samples_per_frame = 512
    };

    struct xma_packet_header {
        Bit_uint<4> sequence_number;
        Bit_uint<2> unk;
        Bit_uint<15> skip_bits;
        Bit_uint<11> packet_skip;

        xma_packet_header(void) :
            sequence_number(0),
            unk(0),
            skip_bits(0),
            packet_skip(0) {}

        friend Bit_stream& operator>> (Bit_stream& bs, xma_packet_header &ph) {
            return bs >> ph.sequence_number >> ph.unk >> ph.skip_bits >> ph.packet_skip;
        }

        friend Bit_ostream& operator<< (Bit_ostream& bs, xma_packet_header &ph) {
            return bs << ph.sequence_number << ph.unk << ph.skip_bits << ph.packet_skip;
        }
    };

    struct xma2_packet_header {
        Bit_uint<6> frame_count;
        Bit_uint<15> skip_bits;
        Bit_uint<3> metadata;
        Bit_uint<8> packet_skip;

        xma2_packet_header(void) :
            frame_count(0),
            skip_bits(0),
            metadata(0),
            packet_skip(0) {}

        friend Bit_stream& operator>> (Bit_stream& bs, xma2_packet_header &ph) {
            return bs >> ph.frame_count >> ph.skip_bits >>
                ph.metadata >> ph.packet_skip;
        }
    };

    struct xma_build_context {
        unsigned int bits_written;  // bits written this packet
        unsigned int seqno;         // sequence number
    };

    /// return sample count
    unsigned int parse_XMA_packets(istream& is, ostream& os, long offset, long data_size, bool stereo, bool strict, bool verbose, bool ignore_packet_skip);
    unsigned int parse_XMA2_block(istream& is, ostream& os, long offset, long block_size, bool stereo, bool strict, bool verbose, bool ignore_packet_skip);
    unsigned int parse_frames(Bit_stream& ps, unsigned int frame_count, bool known_frame_count, unsigned int * total_bits, unsigned int max_bits, bool stereo, bool strict, bool verbose);

    void init_build_XMA(Bit_ostream& bs, xma_build_context * ctx);
    unsigned int build_XMA_from_XMA(istream& is, Bit_ostream& bs, long offset, long data_size, xma_build_context * ctx, bool stereo, bool strict, bool verbose, bool ignore_packet_skip);
    unsigned int build_XMA_from_XMA2_block(istream& is, Bit_ostream& bs, long offset, long block_size, xma_build_context * ctx, bool stereo, bool strict, bool last, bool verbose, bool ignore_packet_skip);
    void packetize(Bit_stream& frame_stream, Bit_ostream& out_stream, xma_build_context * ctx, unsigned int frame_count, bool strict, bool last);
    void finish_build_XMA(Bit_ostream& bs, xma_build_context * ctx);
}

#endif // _XMA_PARSE
