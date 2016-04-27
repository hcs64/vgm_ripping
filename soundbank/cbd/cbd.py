#!/usr/bin/python

# 0.0
# export .dsp from .chd/.cbd (Star Fox Assault)

from sys import argv
from struct import unpack, pack
from io import open

infile_data = open(argv[2], "rb")
infile = open(argv[1], "rb")

infile.seek(0, 2)   # end
infile_size = infile.tell()
infile.seek(0)

infile_data.seek(0, 2)  # end
infile_data_size = infile_data.tell()
infile_data.seek(0)

def samples_to_nibbles(sample_count):
    nibble_count = int(sample_count/14)*16
    if (sample_count%14) > 0:
        nibble_count = nibble_count + 2 + sample_count%14

    return nibble_count

# set up some convenience stuff for file access
def read32():
    return unpack(">I", infile.read(4))[0]
def read16():
    return unpack(">H", infile.read(2))[0]
def read8():
    return ord(infile.read(1)[0])

# parse main header
assert(infile.read(4) == "CHDp")
assert(read16() == 0x0110)
bank_id = read16()

sample_count = read16()

assert(read16() == 0x830)
assert(infile_data_size == read32())

# output samples
for i in range(sample_count):
    outfile_name = "%04x_%02x.dsp" % (bank_id, i)

    data_offset = read32()
    infile.seek(0x50-4, 1)    # cur
    header = infile.read(0x60)

    outfile = open(outfile_name, "wb")
    outfile.write(header)

    infile_data.seek(data_offset)
    data_nibbles = unpack(">I",header[0:4])[0]
    data_size = int((data_nibbles+1)/2)
    data = infile_data.read(data_size)
    outfile.write(data)
    outfile.close()

print("done")
