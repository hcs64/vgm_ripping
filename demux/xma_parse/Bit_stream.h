#ifndef _BIT_STREAM_H
#define _BIT_STREAM_H

#include <iostream>
#include <limits>

namespace {
    uint32_t read_32_be(unsigned char b[4])
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint32_t read_32_be(std::istream &is)
    {
        char b[4];
        is.read(b, 4);

        return read_32_be(reinterpret_cast<unsigned char *>(b));
    }
}

// using an istream, pull off individual bits with get_bit (MSB first)
class Bit_stream {
    std::istream& is;


    const unsigned int consecutive_bits;
    unsigned int consecutive_bits_left;
    const unsigned int skip_bits;

    unsigned char bit_buffer;
    unsigned int bits_left;

public:
    class Weird_char_size {};
    class Out_of_bits {};

    Bit_stream(std::istream& _is,
            unsigned int _consecutive_bits = 0,
            unsigned int _skip_bits = 0
            ) : is(_is), consecutive_bits(_consecutive_bits),
                consecutive_bits_left(_consecutive_bits),
                skip_bits(_skip_bits), bit_buffer(0), bits_left(0) {
        if ( std::numeric_limits<unsigned char>::digits != 8)
            throw Weird_char_size();
    }
    bool get_bit() {
        if (consecutive_bits && consecutive_bits_left == 0) {
            consecutive_bits_left = skip_bits;
            // recursive call to get, yet throw away, bits
            while (consecutive_bits_left) get_bit();
        }

        if (bits_left == 0) {

            int c = is.get();
            if (c == EOF) throw Out_of_bits();
            bit_buffer = c;
            bits_left = 8;

        }
        bits_left --;
        if (consecutive_bits) consecutive_bits_left --;
        return ( ( bit_buffer & ( 1 << bits_left ) ) != 0);
    }
};

class Bit_ostream {
    std::ostream& os;

    unsigned char bit_buffer;
    unsigned int bits_stored;

public:
    class Weird_char_size {};

    Bit_ostream(std::ostream& _os) :
        os(_os), bit_buffer(0), bits_stored(0) {
        if ( std::numeric_limits<unsigned char>::digits != 8)
            throw Weird_char_size();
        }

    void put_bit(bool bit) {
        bit_buffer <<= 1;
        if (bit)
        bit_buffer |= 1;

        bits_stored ++;
        if (bits_stored == 8) {
            flush_bits();
        }
    }

    void flush_bits(void) {
        if (bits_stored != 0) {
            os << bit_buffer;
            bits_stored = 0;
            bit_buffer = 0;
        }
    }

    ~Bit_ostream() {
        flush_bits();
    }
};

// integer of a certain number of bits, to allow reading just that many
// bits from the Bit_stream
template <int BIT_SIZE>
class Bit_uint {
    unsigned int total;
public:
    class Too_many_bits {};
    class Int_too_big {};

    Bit_uint() : total(0) {
        if (BIT_SIZE > std::numeric_limits<unsigned int>::digits)
            throw Too_many_bits();
    }

    explicit Bit_uint(unsigned int v) : total(v) {
        if (v >= (1 << BIT_SIZE)) {
            throw Int_too_big();
        }
    }

    Bit_uint& operator = (unsigned int v) {
        if (v >= (1 << BIT_SIZE)) {
            throw Int_too_big();
        }
        total = v;
        return *this;
    }

    operator unsigned int() { return total; }

    friend Bit_stream& operator >> (Bit_stream& bstream, Bit_uint& bui) {
        bui.total = 0;
        for ( int i = 0; i < BIT_SIZE; i++) {
            bui.total *= 2;
            if ( bstream.get_bit() ) bui.total ++;
        }
        return bstream;
    }

    friend Bit_ostream& operator << (Bit_ostream& bstream, Bit_uint& bui) {
        for ( int i = BIT_SIZE-1; i >= 0; i--) {
            bstream.put_bit((bui.total & (1 << i)) != 0);
        }
        return bstream;
    }
};

#endif // _BIT_STREAM_H
