#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <cstring>
#include "stdint.h"
#include "errors.h"
#include "VID1.h"
#include "Bit_stream.h"

using namespace std;

class Vorbis_packet_header
{
    uint8_t type;

    static const char vorbis_str[6];

public:
    explicit Vorbis_packet_header(uint8_t t) : type(t) {}

    friend Bit_oggstream& operator << (Bit_oggstream& bstream, const Vorbis_packet_header& vph) {
        Bit_uint<8> t(vph.type);
        bstream << t;

        for ( unsigned int i = 0; i < 6; i++ )
        {
            Bit_uint<8> c(vorbis_str[i]);
            bstream << c;
        }

        return bstream;
    }
};

const char Vorbis_packet_header::vorbis_str[6] = {'v','o','r','b','i','s'};

VID1_Vorbis::VID1_Vorbis(const string& name) :
    _file_name(name),
    _infile(name.c_str(), ios::binary), _file_size(-1),
    _vid1_offset(-1), _vid1_size(-1),
    _head_offset(-1), _head_size(-1),
    _first_fram_offset(-1),
    _sample_rate(0), _channels(0), _sample_count(0),
    _info_packet_offset(-1), _info_packet_size(-1),
    _setup_packet_offset(-1), _setup_packet_size(-1)
{
    if (!_infile) throw File_open_error(name);

    _infile.seekg(0, ios::end);
    _file_size = _infile.tellg();


    // read chunks
    long chunk_offset = 0;
    while (chunk_offset < _file_size)
    {
        _infile.seekg(chunk_offset, ios::beg);

        if (chunk_offset + 8 > _file_size) throw Parse_error_str("chunk header truncated");

        char chunk_type[4];
        _infile.read(chunk_type, 4);
        uint32_t chunk_size = read_32_be(_infile);
        uint32_t payload_size = chunk_size - 8;
        long payload_offset = chunk_offset + 8;
        

        if (!memcmp(chunk_type,"VID1",4))
        {
            _vid1_offset = payload_offset;
            _vid1_size = payload_size;
        }
        else if (!memcmp(chunk_type,"HEAD",4))
        {
            if (-1 == _vid1_offset)
            {
                throw Parse_error_str("HEAD before VID1");
            }
            _head_offset = payload_offset;
            _head_size = payload_size;
        }
        else if (!memcmp(chunk_type,"FRAM",4))
        {
            if (-1 == _head_offset)
            {
                throw Parse_error_str("FRAM before HEAD");
            }
            if (-1 == _first_fram_offset)
            {
                _first_fram_offset = chunk_offset;
            }
        }
        else if (!memcmp(chunk_type,"\0\0\0",4) &&
               _file_size - chunk_offset <= 0x20)
        {
            _file_size = chunk_offset;
        }
        else
        {
            throw Parse_error_str("unknown chunk type");
        }

        chunk_offset = chunk_offset + chunk_size;
    }

    if (chunk_offset > _file_size) throw Parse_error_str("chunk truncated");

    // check that we have the chunks we're expecting
    if (-1 == _vid1_offset || -1 == _head_offset || -1 == _first_fram_offset)
    {
        throw Parse_error_str("expected VID1, HEAD, and FRAM chunks");
    }

    // read VID1 chunk
    if (0x18 != _vid1_size) throw Parse_error_str("bad VID1 size");
    _infile.seekg(_vid1_offset, ios::beg);
    {
        // Don't know what any of this means but I aims to find out
        // if any of it changes
        uint16_t VID1_stuff[0xC] = {0,0,0x100,0x100,1,0,0,0,0,0,0,0};
        for (int i = 0; i < 0xC; i++)
        {
            if (read_16_be(_infile) != VID1_stuff[i])
            {
                throw Parse_error_str("Unexepcted stuff in VID1");
            }
        }
    }

    // read head
    _infile.seekg(_head_offset, ios::beg);
    {
        char chunk_type[4];
        uint32_t chunk_size;

        // check pad
        if (0 != read_32_be(_infile))
        {
            throw Parse_error_str("expected 0 padding in HEAD");
        }

        // check AUDH
        _infile.read(chunk_type, 4);
        if (memcmp(chunk_type,"AUDH",4))
        {
            throw Parse_error_str("expected AUDH");
        }
        chunk_size = read_32_be(_infile);
        if (chunk_size > _head_size - 12)
        {
            throw Parse_error_str("AUDH size mismatch");
        }

        // check pad
        if (0 != read_32_be(_infile))
        {
            throw Parse_error_str("expected 0 padding in AUDH");
        }

        // check VAUD (Vorbis audio?)
        _infile.read(chunk_type, 4);
        if (memcmp(chunk_type,"VAUD",4))
        {
            throw Parse_error_str("expected VAUD");
        }

        _sample_rate = read_32_be(_infile);
        _channels = read_8(_infile);
        if (1 != read_8(_infile) || // ?
            0x20 != read_16_be(_infile))  // padding?
        {
            throw Parse_error_str("HEAD crap");
        }
#if 0
            // bitrate crap
            0x22800 != read_32_be(_infile) || // ?
            0x416C != read_32_be(_infile)) // ?
#endif

        read_32_be(_infile);
        read_32_be(_infile);
        read_32_be(_infile); // ?
        read_32_be(_infile); // ?

        if (0xFA != read_32_be(_infile)) // ?
        {
            throw Parse_error_str("HEAD crap 2");
        }

        _sample_count = read_32_be(_infile); // sample count?

        // get identification packet info
        {
            Bit_stream ss(_infile);
            Bit_uint<4> size_bits;
            ss >> size_bits;
            Bit_uintv size(size_bits+1);
            ss >> size;

            _info_packet_size = size;
            _info_packet_offset = _infile.tellg();

            if ( 0x1e != size ||
                 1  != read_8(_infile) ||
                'v' != read_8(_infile) ||
                'o' != read_8(_infile) ||
                'r' != read_8(_infile) ||
                'b' != read_8(_infile) ||
                'i' != read_8(_infile) ||
                's' != read_8(_infile) )
            {
                throw Parse_error_str("bad identification packet");
            }
        }

        // get setup packet info
        {
            _infile.seekg(_info_packet_offset + _info_packet_size);
            Bit_stream ss(_infile);
            Bit_uint<4> size_bits;
            ss >> size_bits;
            Bit_uintv size(size_bits+1);
            ss >> size;

            _setup_packet_size = size;
            _setup_packet_offset = _infile.tellg();

            if ( 5  != read_8(_infile) ||
                'v' != read_8(_infile) ||
                'o' != read_8(_infile) ||
                'r' != read_8(_infile) ||
                'b' != read_8(_infile) ||
                'i' != read_8(_infile) ||
                's' != read_8(_infile) )
            {
                throw Parse_error_str("bad setup packet");
            }
        }
    }
}

void VID1_Vorbis::generate_ogg_header(Bit_oggstream& os)
{
    // copy information packet
    {
        _infile.seekg(_info_packet_offset);

        Bit_uint<8> c(_infile.get());
        if (1 != c)
        {
            throw Parse_error_str("wrong type for information packet");
        }

        os << c;

        for (unsigned int i = 1; i < _info_packet_size; i++)
        {
            c = _infile.get();
            os << c;
        }

        // identification packet on its own page
        os.end_packet();
        os.end_page();
    }

    // generate comment packet
    {
        Vorbis_packet_header vhead(3);

        os << vhead;

        static const char vendor[] = "converted by vid1_2ogg " VERSION;
        Bit_uint<32> vendor_size(strlen(vendor));

        os << vendor_size;
        for (unsigned int i = 0; i < vendor_size; i ++) {
            Bit_uint<8> c(vendor[i]);
            os << c;
        }

        // no user comments
        Bit_uint<32> user_comment_count(0);
        os << user_comment_count;

        Bit_uint<1> framing(1);
        os << framing;

        os.end_packet();
    }

    // copy setup packet
    {
        _infile.seekg(_setup_packet_offset);
        Bit_stream ss(_infile);

        Bit_uint<8> c;
        ss >> c;

        // type
        if (5 != c)
        {
            throw Parse_error_str("wrong type for setup packet");
        }
        os << c;

        for (unsigned int i = 1; i < _setup_packet_size; i++)
        {
            c = _infile.get();
            os << c;
        }

        os.end_packet();
    }

    // end of header pages
    os.end_page();
}

void VID1_Vorbis::generate_ogg(ofstream& of)
{
    Bit_oggstream os(of);

    generate_ogg_header(os);

    // Audio pages
    {
        long offset = _first_fram_offset;
        long granule = 0;

        bool first_page = true;

        while (offset < _file_size)
        {
            uint32_t chunk_size, payload_size;

            if (first_page)
            {
                first_page = false;
            }
            else
            {
                //granule++;
                os.end_page();
            }

            {
                _infile.seekg(offset);

                char chunk_type[4];
                _infile.read(chunk_type, 4);

                if (memcmp(chunk_type,"FRAM",4))
                {
                    if (memcmp(chunk_type,"\0\0\0",4))
                    {
                        break;
                    }

                    throw Parse_error_str("missing FRAM");
                }

                chunk_size = read_32_be(_infile);

                // check padding
                for (int i = 0; i < 0x20-8; i+=4)
                {
                    if (0 != read_32_be(_infile))
                    {
                        throw Parse_error_str("nonzero padding in FRAM");
                    }
                }

                _infile.read(chunk_type, 4);
                if (memcmp(chunk_type, "AUDD", 4))
                {
                    throw Parse_error_str("missing AUDD");
                }

                if (read_32_be(_infile) != chunk_size - 0x20)
                {
                    throw Parse_error_str("FRAM/AUDD size mismatch");
                }

                if (0 != read_32_be(_infile))
                {
                    throw Parse_error_str("nonzero padding in AUDD");
                }

                payload_size = read_32_be(_infile);

                if (payload_size > chunk_size - 0x30)
                {
                    throw Parse_error_str("doesn't seem like payload fits in FRAM");
                }
            }

            {
                long packet_offset = offset + 0x32;

                _infile.seekg(packet_offset);

                granule += read_16_be(_infile);

                packet_offset += 2;

                while (packet_offset < offset + 0x30 + payload_size)
                {
                    os.set_granule(granule);

                    Bit_stream is(_infile);
                    Bit_uint<4> size_bits;
                    is >> size_bits;
                    Bit_uintv packet_size(size_bits+1);
                    is >> packet_size;

                    int header_bytes = (4+size_bits+1+7)/8;
                    if (0 == size_bits)
                    {
                        _infile.seekg(packet_offset);
                        if (0x80 == _infile.get())
                        {
                            packet_size = 1;
                        }
                    }
                    _infile.seekg(packet_offset + header_bytes);
                    for (unsigned int i = 0; i < packet_size; i++)
                    {
                        Bit_uint<8> c(_infile.get());
                        os << c;
                    }

                    packet_offset += header_bytes + packet_size;

                    if (packet_size != 0)
                    {
                        os.end_packet();
                    }
                }

                if (packet_offset != offset + 0x30 + payload_size)
                {
                    throw Parse_error_str("packets didn't match up with AUDD payload size");
                }
            }

            offset = offset + chunk_size;
        }

        if (offset > _file_size) throw Parse_error_str("page truncated");

        if (granule != _sample_count) throw Parse_error_str("miscounted samples");
    }
}
