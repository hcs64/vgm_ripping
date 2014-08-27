#ifndef _VID1_H
#define _VID1_H

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <string>
#include <iostream>
#include <fstream>
#include "Bit_stream.h"
#include "stdint.h"
#include "errors.h"

#define VERSION "0.2"

using namespace std;

class VID1_Vorbis
{
    string _file_name;
    ifstream _infile;
    long _file_size;
    long _vid1_offset, _vid1_size;
    long _head_offset, _head_size;
    long _first_fram_offset;

    uint32_t _sample_rate;
    uint8_t _channels;
    uint32_t _sample_count;

    long _info_packet_offset, _info_packet_size;
    long _setup_packet_offset, _setup_packet_size;

public:
    VID1_Vorbis(const string& name);

    void generate_ogg(ofstream& of);
    void generate_ogg_header(Bit_oggstream& os);
};

#endif
