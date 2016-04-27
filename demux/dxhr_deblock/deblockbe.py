#!/usr/bin/python
# unblock the packing used in Deus Ex Human Revolution (release)
# builds an XMA header for 360 version
# tested with python 2.6.5

from io import open
from struct import unpack, pack
from os import SEEK_CUR, SEEK_SET
from sys import argv

if len(argv) != 3:
    print("usage: deblockbe.py [ps3 | 360] infile")
    exit()

platform = argv[1]
infile_name = argv[2]

def write_XMA_header(outfile, srate, channels, size):

    # RIFF header
    outfile.write("RIFF")
    outfile.write(pack("<I", size + 0x3c - 8))  # RIFF size
    outfile.write("WAVE")

    # fmt
    outfile.write("fmt ")
    outfile.write(pack("<IHHHHHBB",0x20, 0x165, 16, 0, 0, 1, 0, 3)) # fmt chunk size, WAVE_FORMAT_XMA, 16 bps, no end options, no skip, 1 stream, 0 loops, env ver 3

    # lone stream info
    outfile.write(pack("<IIIIBBH", 0, srate, 0, 0, 0, channels, 0x0002)) # 0 bytes per second, srate, loop start, loop end, subframe loop, channels, channel mask

    # data chunk
    outfile.write("data")
    outfile.write(pack("<I", size)) # data chunk size

infile = open(infile_name, "rb")

output_ext = ""
if platform == "ps3":
    output_ext = ".fsb"
elif platform == "360":
    output_ext = ".xma"
else:
    print("unknown platform %s" % platform)
    exit()

total_channels = 0
srate = 0
samples = 0

# Read header
if infile.read(4) == "Mus!":
    infile.seek(0x1C, SEEK_SET)
    stream_count, unk1, stream_info_offset, unk2, stream_platform_offset = unpack(">IIIII", infile.read(20))


    # read each stream's info
    infile.seek(stream_info_offset, SEEK_SET)
    stream_info = {}

    for i in range(stream_count):
        srate, zero1, channels, zero2, samples, unk1 = unpack(">IIIIII", infile.read(24))
        assert(zero1 == 0 and zero2 == 0)
        infile.read(64) # volume control stuff
        print("stream %d: %d Hz, %d channels, %d samples" % (i, srate, channels, samples))
        stream_info[i] = {'srate' : srate, 'channels' : channels, 'samples' : samples}
        total_channels += channels

    curr_off = infile.tell()

    data_header_off = (curr_off + 0x800 - 1) / 0x800 * 0x800

    # read data header
    infile.seek(data_header_off, SEEK_SET)
    srate, unk1, samples, channels = unpack(">IIII", infile.read(16))
    assert(channels == total_channels)

    for i in range(stream_count):
        assert(srate == stream_info[i]['srate'])
        assert(samples == stream_info[i]['samples'])

else:
    # read data header
    data_header_off = 0
    infile.seek(data_header_off, SEEK_SET)

    srate, unk1, samples, total_channels = unpack(">IIII", infile.read(16))

curr_off = infile.tell()
data_off = (curr_off + 0x800 - 1) / 0x800 * 0x800
# read data

infile.seek(data_off, SEEK_SET)

outfiles = {}
for i in range(total_channels):
    file_name = "%s_%d%s" % (infile_name, i, output_ext)
    outfiles[i] = open(file_name, "wb")
    print("opened %s for output" % file_name)

    if platform == "360":
        outfiles[i].seek(0x3c, SEEK_SET)

block_payload_left = 0

while 1:
    #print("%x %x" % (infile.tell(), block_payload_left))

    if block_payload_left == 0:
        # need a new count header
        pad_amt = 16-(infile.tell() % 16)
        if pad_amt < 16:
            infile.seek(pad_amt, SEEK_CUR)

        header_bytes = infile.read(16)
        zero1, block_payload_left, sampleoff, zero3 = unpack(">IIII", header_bytes)

        assert(zero1 == 0 and zero3 == 0)

        # next block is empty, give up
        if block_payload_left == 0:
            print("hit empty block, seemingly done!")
            break

    else:
        header_bytes = infile.read(16)
        block_size, stream_id, unk1, unk2 = unpack(">IIII", header_bytes)

        assert(unk1 == 0x2001)
        assert(unk2 == 0)
        assert(stream_id < total_channels)

        block_bytes = infile.read(block_size)
        assert(len(block_bytes) == block_size)

        outfiles[stream_id].write(block_bytes)

        block_payload_left -= 16 + block_size


# update XMA header now that we know file size
if (platform == "360"):
    for i in range(total_channels):
        size = outfiles[i].tell() - 0x3c
        outfiles[i].seek(0, SEEK_SET)
        write_XMA_header(outfiles[i], srate, 1, size)


