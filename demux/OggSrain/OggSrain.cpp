#define __STDC_CONSTANT_MACROS

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <sstream>
#include <map>
#include "Bit_stream.h"

using namespace std;

int main(int argc, char ** argv)
{
    int verbose_level = 0;
    const int verbose_level_streams = 1;
    const int verbose_level_detail = 2;

    cout << "OggS Rain on Brooklyn 0.0" << endl << endl;

    if (2 != argc)
    {
        cout << "usage: " << argv[0] << " in.mediastream_s" << endl;
        cout << endl
            << "\"Really, it's just a question of reassembling the components"
            << " in the " << endl << "   correct sequence...\""
            << " - Watchmen" << endl;

        return 1;
    }

    ifstream infile(argv[1], ios::binary);
    if (!infile)
    {
        cout << "error opening " << argv[1] << endl;
        return 1;
    }

    map<uint16_t, Oggstream *> streams;

    uint16_t stream_id = 0;

    while (!infile.eof())
    {
        Oggstream * oggstream;
        bool stream_known = false;
        unsigned int stream_confidence = 0;
        const unsigned int stream_confidence_threshold = 12;

        if (0 == stream_id)
        {
            ostringstream outname;
            outname << argv[1] << "_" << streams.size() << ".ogg";

            cout << "Writing Ogg to " << outname.str() << endl;

            oggstream = new Oggstream(outname.str().c_str());
        }
        else
        {
            if (verbose_level_streams <= verbose_level)
            {
                cout << "Switch back to stream 0x"
                     << hex << stream_id << dec << endl;
            }

            if (streams.count(stream_id) == 0)
            {
                cout << "Stream 0x"
                     << hex << stream_id << dec << " not known" << endl;
                return 1;
            }

            stream_known = true;

            oggstream = streams[stream_id];
        }

        while (!infile.eof())
        {
            if (verbose_level_detail <= verbose_level)
            {
                cout << "Input offset 0x"
                     << hex << infile.tellg() << dec << endl;
            }

            char head_buf[0x20];
            infile.read(head_buf, 4);
            unsigned char * const uchar_head_buf =
                reinterpret_cast<unsigned char*>(head_buf);

            uint32_t head_num1 = read_16_le(&uchar_head_buf[0]);
            uint32_t head_num2 = read_16_le(&uchar_head_buf[2]);

            if (verbose_level_detail <= verbose_level)
            {
                cout << "Head: 0x"
                     << hex << head_num1 << " 0x" << head_num2 << dec << endl;
            }

            // skip these guys, cue points?
            if ((head_num2&~7) == 0)
            {
                if (verbose_level_detail <= verbose_level)
                {
                    cout << "cue type " << head_num2 << "?" << endl << endl;
                }
                continue;
            }

            infile.read(&head_buf[4], 28);

            uint32_t payload_bytes = read_32_le(&uchar_head_buf[4]);
            uint32_t first = read_32_le(&uchar_head_buf[8]);
            uint32_t last = read_32_le(&uchar_head_buf[12]);
            uint32_t granule = read_32_le(&uchar_head_buf[16]);
            uint32_t seqno = read_32_le(&uchar_head_buf[24]);

            if (verbose_level_detail <= verbose_level)
            {
                cout << "Seq " << seqno << endl;
                cout << "Payload " << payload_bytes << " bytes" << endl;
                cout << "Granule " << granule << endl;
            }
            
            if (1 == first)
            {
                if (verbose_level_detail <= verbose_level)
                {
                    cout << "First!" << endl;
                }
            }
            else
            {
                if (0 != first)
                {
                    cout << "First flag = " << first << endl;
                }
            }

            if (1 == last)
            {
                if (verbose_level_detail <= verbose_level)
                {
                    cout << "Last!" << endl;
                }
            }
            else
            {
                if (0 != last)
                {
                    cout << "Last flag = " << last << endl;
                }
            }

            for (unsigned int i = 12; i < 32; i += 4)
            {
                uint32_t v = read_32_le(&uchar_head_buf[i]);
                if (0 != v && 12 != i && 16 != i && 24 != i)
                {
                    cout << "head[" << i << "] = 0x" << hex << v << dec << endl;
                }
            }

            if (stream_known && stream_id != (head_num2&~7))
            {
                if (seqno == 0)
                {
                    stream_id = 0;

                    if (verbose_level_streams <= verbose_level)
                    {
                        cout << "start of a new stream!" << endl;
                    }
                }
                else
                {
                    stream_id = (head_num2&~7);

                    if (streams.count(stream_id) == 0)
                    {
                        cout << "Stream 0x"
                             << hex << stream_id << dec << " not known" << endl;
                        return 1;
                    }
                    if (verbose_level_streams <= verbose_level)
                    {
                        cout << "switched to an old stream: 0x"
                             << hex << stream_id << dec << endl;
                    }
                }
                infile.seekg(-32, ios::cur);
                break;
            }

            if (oggstream->get_seqno() != seqno)
            {
                cout << "Bad seqno!" << endl;
                return 1;
            }

            if (oggstream->get_saw_last())
            {
                cout << "Payload after end of stream!" << endl;
                return 1;
            }

            if (0 != first && 0 != oggstream->get_seqno())
            {
                cout << "First flag set but seqno > 0" << endl;
                return 1;
            }

            if (0 == first && 0 == oggstream->get_seqno())
            {
                cout << "First flag not set on seqno == 0" << endl;
                return 1;
            }

            char *payload = new char [payload_bytes];

            infile.read(payload, payload_bytes);

            oggstream->write_page(reinterpret_cast<unsigned char *>(payload),
                    payload_bytes, granule, first, last );

            delete payload;

            if (!stream_known)
            {
                // if we see the same id enough times we'll believe it
                if (stream_id == (head_num2&~7))
                {
                    stream_confidence ++;
                    if (stream_confidence_threshold <= stream_confidence)
                    {
                        stream_known = true;
                        if (verbose_level_streams <= verbose_level)
                        {
                            cout << "identified stream 0x"
                                 << hex << stream_id << dec << endl;
                        }

                        if (streams.count(stream_id) != 0)
                        {
                            cout << "Stream 0x"
                                 << hex << stream_id << dec
                                 << " already known" << endl;
                            return 1;
                        }

                        streams[stream_id] = oggstream;
                    }
                }
                else
                {
                    stream_confidence = 0;
                }

                stream_id = (head_num2&~7);
            }

            if (verbose_level_detail <= verbose_level)
            {
                cout << endl;
            }
        }

        // finished with a stream (for now anyway)
        if (!stream_known)
        {
            cout << "Stream ended unidentified" << endl;
            return 1;
        }
    }

    {
        map<uint16_t, Oggstream *>::iterator it;

        for ( it = streams.begin(); it != streams.end(); it++ )
        {
            if (!it->second->get_saw_last())
            {
                cout << "Stream 0x"
                     << hex << it->first << dec
                     << " incomplete!" << endl;

                return 1;
            }

            if (verbose_level_streams <= verbose_level)
            {
                cout << "Closing stream 0x"
                     << hex << it->first << dec << endl;
            }

            delete it->second;
        }
    }

    cout << "Done!" << endl;

    return 0;
}
