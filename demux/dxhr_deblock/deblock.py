#!/usr/bin/python
# unblock the packing used in Deus Ex Human Revolution (beta)

from io import open
from struct import unpack
from os import SEEK_CUR, SEEK_SET
from sys import argv

offsetstr = ""

if len(argv) >= 2:
    infile = open(argv[1], "rb")

if len(argv) == 3:
    offset = int(argv[2],16)
    offsetstr = "_%x" % offset
    infile.seek(offset, SEEK_SET)
elif len(argv) != 2:
    print("usage: deblock.py infile [hex offset]")
    exit()

outfiles = {}

block_payload_left = 0

while 1:
    #print("%x %x" % (infile.tell(), block_payload_left))

    if block_payload_left == 0:
        # need a new count header
        pad_amt = 16-(infile.tell() % 16)
        if pad_amt < 16:
            infile.seek(pad_amt, SEEK_CUR)

        header_bytes = infile.read(16)
        zero1, block_payload_left, zero2, zero3 = unpack("<IIII", header_bytes)

        assert(zero1 == 0 and zero2 == 0 and zero3 == 0)

        # next block is empty, give up
        if block_payload_left == 0:
            print("hit empty block, seemingly done!")
            break

    else:
        header_bytes = infile.read(16)
        block_size, stream_id, unk1, unk2 = unpack("<IIII", header_bytes)

        assert(unk1 == 0x2001)
        assert(unk2 == 0)

        block_bytes = infile.read(block_size)
        assert(len(block_bytes) == block_size)

        if not stream_id in outfiles:
            file_name = "%s%s_%08x.fsb" % (argv[1], offsetstr, stream_id)
            outfiles[stream_id] = open(file_name, "wb")
            print("opened %s for output" % file_name)

        outfiles[stream_id].write(block_bytes)

        block_payload_left -= 16 + block_size
