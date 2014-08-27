#include <fstream>
#include <sstream>
#include <stdlib.h>

#include "xma_parse.h"
#include "Bit_stream.h"

using namespace std;

namespace Get_arguments {

struct Argument_error {
    string description;
    Argument_error(string desc) : description(desc) {}
};

void get(int argc, char * argv[], const char ** filename, const char ** out_filename, const char ** rebuild_filename, long *offset, long *block_size, long *data_size, int *channels, int *version, bool *strict, bool *verbose, bool *ignore_packet_skip);
}

int main(int argc, char * argv[]) {
    const char * input_filename, * output_filename, * rebuild_filename;
    long offset, block_size, data_size;
    int channels;
    bool strict;
    bool verbose;
    bool ignore_packet_skip;
    int version;

    cout << "XMA/XMA2 stream parser 0.11 by hcs" << endl << endl << flush;

    try {
        Get_arguments::get(argc, argv, &input_filename, &output_filename, &rebuild_filename, &offset, &block_size, &data_size, &channels, &version, &strict, &verbose, &ignore_packet_skip);
    } catch (Get_arguments::Argument_error ae) {
        cerr << ae.description << endl
<< "usage: xma_parse filename [-1|-2] [-o offset] [-b block size] [-d data size]" << endl
<< "       [-x output filename] [-r output filename] [-s] [-v]" << endl << endl
<< "    filename is the input file" << endl
<< "    -1/-2 indicate that the input is XMA(1) or XMA2, default XMA2" << endl
<< "    -o is the offset to start parsing, default 0" << endl
<< "    -b is the block size to use for XMA2, default 8000" << endl
<< "    -d is bytes from offset that are XMA data, default rest of file" << endl
<< "    -x output file for dumping" << endl
<< "    -r output file for rebuilding to XMA(1)" << endl
<< "    -s strict processing, don't attempt to fix some issues" << endl
<< "    -I ignore packet skip" << endl
<< "    -B XMA2 block size is equal to data size (no real blocking)" << endl
<< "    -v indicates verbose output, lots of stream structure and content" << endl
<< endl;
        exit(EXIT_FAILURE);
    }

    // Input file
    ifstream is(input_filename, ios::binary);

    if (!is) {
        cerr << "error opening file!" << endl;
        exit(EXIT_FAILURE);
    }

    if (-1 == data_size) {
        is.seekg(0, ios::end);
        data_size = static_cast<long>(is.tellg()) - offset;
    }

    if (-1 == block_size) {
        block_size = data_size;
    }

    // Output file for exported stream
    ofstream os;

    if (NULL != output_filename)
    {
        os.open(output_filename, ios::binary);

        if (!os) {
            cerr << "error opening output file!" << endl;
            exit(EXIT_FAILURE);
        }
    }

    // Output file for rebuilt stream
    ofstream rs;

    if (NULL != rebuild_filename)
    {
        rs.open(rebuild_filename, ios::binary);

        if (!rs) {
            cerr << "error opening rebuild output file!" << endl;
            exit(EXIT_FAILURE);
        }
    }

    cout << "filename: " << input_filename << endl;
    cout << "version: ";
    if (version == 1) {
        cout << "Original XMA" << endl;
    } else {
        cout << "XMA2" << endl;
    }

    cout << "offset: " << hex << offset << dec << endl;
    if (version == 2) {
        cout << "block size: " << hex << block_size << dec << endl;
    }
    cout << "data size: " << hex << data_size << dec << endl;
    if (os.is_open()) {
        cout << "output filename: " << output_filename << endl;
    }
    if (rs.is_open()) {
        cout << "rebuild output filename: " << rebuild_filename << endl;
    }
    if (strict) {
        cout << "strict checking enabled" << endl;
    }
    if (ignore_packet_skip) {
        cout << "ignoring packet skip" << endl;
    }
    cout << "------------------" << endl << endl;

    try {
        unsigned long total_sample_count = 0;

        Bit_ostream out_bitstream(rs);

        Parse_XMA::xma_build_context ctx;
        if (rs.is_open()) {
            init_build_XMA(out_bitstream, &ctx);
        }

        if (version == 1) {
            if (!rs.is_open() || os.is_open())
            {
                total_sample_count = Parse_XMA::parse_XMA_packets(is, os, offset, data_size, (channels > 1), strict, verbose, ignore_packet_skip);
            }
            
            if (rs.is_open()) {
                total_sample_count = Parse_XMA::build_XMA_from_XMA(is, out_bitstream, offset, data_size, &ctx, (channels > 1), strict, verbose, ignore_packet_skip);
            }
        } else {
            for (long block_offset = offset;
                    block_offset < offset + data_size;
                    block_offset += block_size ) {

                long usable_block_size = block_size;

                if (block_offset + usable_block_size > offset + data_size)
                {
                    usable_block_size = offset + data_size - block_offset;
                }

                unsigned long sample_count = 0;

                if (!rs.is_open() || os.is_open()) {
                    sample_count = Parse_XMA::parse_XMA2_block(is, os, block_offset, usable_block_size, (channels > 1), strict, verbose, ignore_packet_skip);
                }

                if (rs.is_open()) {
                    sample_count = Parse_XMA::build_XMA_from_XMA2_block(is, out_bitstream, block_offset, usable_block_size, &ctx,
                            (channels > 1), strict, (block_offset + block_size >= offset + data_size), verbose, ignore_packet_skip);
                }

                total_sample_count += sample_count;
                if (verbose) {
                    cout << endl << sample_count << " samples (block) (" << total_sample_count << " total)" << endl << endl;
                }
            }
        }

        if (rs.is_open()) {
            finish_build_XMA(out_bitstream, &ctx);
        }

        cout << endl << total_sample_count << " samples (total)" << endl;
    }
    catch (const Bit_stream::Out_of_bits& oob) {
        cerr << "error reading bitstream" << endl;
        exit(EXIT_FAILURE);
    }
    catch (const Parse_XMA::Parse_error& pe) {
        cerr << pe << endl;
        exit(EXIT_FAILURE);
    }
}

void Get_arguments::get(int argc, char * argv[], const char ** filename, const char ** out_filename, const char ** rebuild_filename, long *offset, long *block_size, long *data_size, int *channels, int *version, bool *strict, bool *verbose, bool *ignore_packet_skip) {
    if (argc < 2) {
        throw Argument_error("missing file name");
    }
    *filename = argv[1];
    *out_filename = NULL;
    *rebuild_filename = NULL;
    *offset = 0;
    *block_size = 0x8000;
    *data_size = -1;
    *channels = 1;
    *version = 2;
    *strict = false;
    *verbose = false;
    *ignore_packet_skip = false;

    for (int argno = 2; argno < argc; argno++) {
        if (argv[argno][0] == '-' && argv[argno][1] != '\0' &&
                argv[argno][2] == '\0') {
            switch (argv[argno][1]) {
                case '1':
                    *version = 1;
                    *block_size = Parse_XMA::packet_size_bytes;
                    continue;
                case '2':
                    *version = 2;
                    continue;
                case 'o':
                    if (argno + 1 >= argc) {
                        throw Argument_error("-o needs an argument");
                    }

                    {
                        stringstream argstring(argv[++argno]);
                        argstring >> hex >> *offset;
                        /* TODO: check that conversion went ok */
                    }
                    continue;
                case 'b':
                    if (argno + 1 >= argc) {
                        throw Argument_error("-b needs an argument");
                    }

                    {
                        stringstream argstring(argv[++argno]);
                        argstring >> hex >> *block_size;
                        /* TODO: check that conversion went ok */
                    }
                    continue;
                case 'c':
                    if (argno + 1 >= argc) {
                        throw Argument_error("-c needs an argument");
                    }
                    {
                        stringstream argstring(argv[++argno]);
                        argstring >> *channels;
                        /* TODO: check that conversion went ok */
                    }
                    continue;
                case 'd':
                    if (argno + 1 >= argc) {
                        throw Argument_error("-d needs an argument");
                    }
                    {
                        stringstream argstring(argv[++argno]);
                        argstring >> hex >> *data_size;
                        /* TODO: check that conversion went ok */
                    }
                    continue;
                case 'x':
                    if (argno + 1 >= argc) {
                        throw Argument_error("-x needs an argument");
                    }
                    *out_filename = argv[++argno];
                    continue;
                case 'r':
                    if (argno + 1 >= argc) {
                        throw Argument_error("-r needs an argument");
                    }
                    *rebuild_filename = argv[++argno];
                    continue;
                case 's':
                    *strict = true;
                    continue;
                case 'v':
                    *verbose = true;
                    continue;
                case 'I':
                    *ignore_packet_skip = true;
                    continue;
                case 'B':
                    *block_size = -1;
                    continue;
                default:
                    throw Argument_error(
                            string("don't know what to do with argument \"") + argv[argno] + "\"");
            }
        }

        throw Argument_error(
                string("don't know what to do with argument \"") + argv[argno] + "\"");
    }
}

