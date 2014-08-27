#ifndef _BIT_STREAM_H
#define _BIT_STREAM_H

#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <fstream>
#include <limits>
#include <stdint.h>

#include "crc.h"

// host-endian-neutral integer reading
namespace {
    uint32_t read_32_le(unsigned char b[4])
    {
        uint32_t v = 0;
        for (int i = 3; i >= 0; i--)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint32_t read_32_le(std::istream &is)
    {
        char b[4];
        is.read(b, 4);

        return read_32_le(reinterpret_cast<unsigned char *>(b));
    }

    void write_32_le(unsigned char b[4], uint32_t v)
    {
        for (int i = 0; i < 4; i++)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_32_le(std::ostream &os, uint32_t v)
    {
        char b[4];

        write_32_le(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 4);
    }

    uint16_t read_16_le(unsigned char b[2])
    {
        uint16_t v = 0;
        for (int i = 1; i >= 0; i--)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint16_t read_16_le(std::istream &is)
    {
        char b[2];
        is.read(b, 2);

        return read_16_le(reinterpret_cast<unsigned char *>(b));
    }

    void write_16_le(unsigned char b[2], uint16_t v)
    {
        for (int i = 0; i < 2; i++)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_16_le(std::ostream &os, uint16_t v)
    {
        char b[2];

        write_16_le(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 2);
    }

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

    void write_32_be(unsigned char b[4], uint32_t v)
    {
        for (int i = 3; i >= 0; i--)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_32_be(std::ostream &os, uint32_t v)
    {
        char b[4];

        write_32_be(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 4);
    }

    uint16_t read_16_be(unsigned char b[2])
    {
        uint16_t v = 0;
        for (int i = 0; i < 2; i++)
        {
            v <<= 8;
            v |= b[i];
        }

        return v;
    }

    uint16_t read_16_be(std::istream &is)
    {
        char b[2];
        is.read(b, 2);

        return read_16_be(reinterpret_cast<unsigned char *>(b));
    }

    void write_16_be(unsigned char b[2], uint16_t v)
    {
        for (int i = 1; i >= 0; i--)
        {
            b[i] = v & 0xFF;
            v >>= 8;
        }
    }

    void write_16_be(std::ostream &os, uint16_t v)
    {
        char b[2];

        write_16_be(reinterpret_cast<unsigned char *>(b), v);

        os.write(b, 2);
    }

}

class Oggstream {
    std::ofstream os;

    enum {header_bytes = 27, max_segments = 255, segment_size = 255};

    unsigned char page_buffer[header_bytes + max_segments + segment_size * max_segments];
    uint32_t seqno;

    bool saw_last;

public:
    explicit Oggstream(const char * filename) : os(filename, std::ios::binary), seqno(0), saw_last(false) {}

    uint32_t get_seqno(void) const {return seqno;}
    bool get_saw_last(void) const {return saw_last;}

    void write_page(unsigned char payload[], long payload_bytes, uint32_t granule, bool first, bool last) {
        unsigned int segments = (payload_bytes+segment_size)/segment_size;  // intentionally round up
        if (segments == max_segments+1) segments = max_segments; // at max eschews the final 0

        // copy payload
        for (unsigned int i = 0; i < payload_bytes; i++)
        {
            page_buffer[header_bytes + segments + i] = payload[i];
        }

        page_buffer[0] = 'O';
        page_buffer[1] = 'g';
        page_buffer[2] = 'g';
        page_buffer[3] = 'S';
        page_buffer[4] = 0; // stream_structure_version
        page_buffer[5] = (first?2:0)|(last?4:0);    // flags
        write_32_le(&page_buffer[6], granule);  // granule low bits
        write_32_le(&page_buffer[10], 0);       // granule high bits
        if (granule == UINT32_C(0xFFFFFFFF))
            write_32_le(&page_buffer[10], UINT32_C(0xFFFFFFFF));
        write_32_le(&page_buffer[14], 1);       // stream serial number
        write_32_le(&page_buffer[18], seqno);   // page sequence number
        write_32_le(&page_buffer[22], 0);       // checksum (0 for now)
        page_buffer[26] = segments;             // segment count

        // lacing values
        for (unsigned int i = 0, bytes_left = payload_bytes; i < segments; i++)
        {
            if (bytes_left >= segment_size)
            {
                bytes_left -= segment_size;
                page_buffer[27 + i] = segment_size;
            }
            else
            {
                page_buffer[27 + i] = bytes_left;
            }
        }

        // checksum
        write_32_le(&page_buffer[22],
                checksum(page_buffer, header_bytes + segments + payload_bytes)
                );

        // output to ostream
        for (unsigned int i = 0; i < header_bytes + segments + payload_bytes; i++)
        {
            os.put(page_buffer[i]);
        }

        if (last) saw_last = true;
        seqno++;
    }
};

#endif // _BIT_STREAM_H
