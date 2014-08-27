#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <fstream>
#include <cstring>
#include "VID1.h"
#include "stdint.h"
#include "errors.h"

using namespace std;

class vid1_2ogg_options
{
    string in_filename;
    string out_filename;
public:
    vid1_2ogg_options(void) : in_filename(""), out_filename("") {}
    void parse_args(int argc, char **argv);
    const string& get_in_filename(void) const {return in_filename;}
    const string& get_out_filename(void) const {return out_filename;}
};

void usage(void)
{
    cout << endl;
    cout << "usage: vid1_2ogg input.ogg [-o output.ogg]" << endl << endl;
}

int main(int argc, char **argv)
{
    cout << "Neversoft VID1 to Ogg converter " VERSION " by hcs" << endl << endl;

    vid1_2ogg_options opt;

    try
    {
        opt.parse_args(argc, argv);
    }
    catch (const Argument_error& ae)
    {
        cout << ae << endl;

        usage();
        return 1;
    }

    try
    {
        cout << "Input: " << opt.get_in_filename() << endl;
        VID1_Vorbis vid1(opt.get_in_filename());

        cout << "Output: " << opt.get_out_filename() << endl;

        ofstream of(opt.get_out_filename().c_str(), ios::binary);
        if (!of) throw File_open_error(opt.get_out_filename());

        vid1.generate_ogg(of);
        cout << "Done!" << endl << endl;
    }
    catch (const File_open_error& fe)
    {
        cout << fe << endl;
        return 1;
    }
    catch (const Parse_error& pe)
    {
        cout << pe << endl;
        return 1;
    }

    return 0;
}

void vid1_2ogg_options::parse_args(int argc, char ** argv)
{
    bool set_input = false, set_output = false;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-o"))
        {
            // switch for output file name
            if (i+1 >= argc)
            {
                throw Argument_error("-o needs an option");
            }

            if (set_output)
            {
                throw Argument_error("only one output file at a time");
            }

            out_filename = argv[++i];
            set_output = true;
        }
        else
        {
            // assume anything else is an input file name
            if (set_input)
            {
                throw Argument_error("only one input file at a time");
            }

            in_filename = argv[i];
            set_input = true;
        }
    }

    if (!set_input)
    {
        throw Argument_error("input name not specified");
    }

    if (!set_output)
    {
        size_t found = in_filename.find_last_of('.');

        out_filename = in_filename.substr(0, found);
        out_filename.append(".ogg");

        if (out_filename == in_filename)
        {
            out_filename.append("_conv.ogg");
        }
    }
}
