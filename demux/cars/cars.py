#!/usr/bin/python

# for THQ's "Cars" (PC version)
# see http://forum.xentax.com/viewtopic.php?f=15&t=2135

from io import open
from sys import argv
from struct import unpack, pack
from math import floor

IMA_Scale = \
[
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
]

IMA_Step = \
[
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8 
]

infile = open(argv[1], "rb")
outfile = open(argv[2], "wb")

def read32(): return unpack("<I",infile.read(4))[0]
def read16(): return unpack("<H",infile.read(2))[0]
def write32(x): outfile.write(pack("<I",x))
def write16(x): outfile.write(pack("<H",x))

riff_head = infile.read(4)
if riff_head != "RIFF":
    raise Exception, "expected RIFF"
file_len = read32()
wavefmt = infile.read(8)
if wavefmt != "WAVEfmt ":
    raise Exception, "expected WAVE fmt"
header_len = read32()
if header_len != 0x14:
    raise Exception, "expected head len 0x14"
codec = read16()
if codec != 2:
    raise Exception, "expected codec 2"
channels = read16()
if channels != 2:
    raise Exception, "expected stereo"
sample_rate = read32()
byte_rate = read32()
sample_size = read16()
if sample_size != 4:
    raise Exception, "expected 4 bit samples"
block_size = read16()
if block_size != 16:
    raise Exception, "expected block size 16"
unk = read32()
if unk != 0x40*0x10000:
    raise Exception, "expecting 00 00 40 00"

data_head = infile.read(4)
if data_head != "data":
    raise Exception, "expected data chunk"
data_len = read32()

data = infile.read(data_len)
payload_size = len(data)
if payload_size != data_len:
    raise Exception, "didn't read all the data?"
sample_count = payload_size
infile.close()

# write RIFF header
outfile.write("RIFF")
write32(payload_size*4+0x24)  # total size
outfile.write("WAVEfmt ")

write32(0x10)   # fmt chunk size
write16(0x1)   # pcm codec id
write16(channels)
write32(sample_rate)
write32(sample_rate*channels*2)
write16(2*channels)  # block align
write16(16)  # bits per sample

outfile.write("data")
write32(payload_size*4)

hist = [0,0]
idx = [0,0]

def decodeima(n,c):
    global idx, hist
    stepsize = IMA_Scale[idx[c]]

    delta = stepsize >> 3
    if (n&1): delta += stepsize >> 2
    if (n&2): delta += stepsize >> 1
    if (n&4): delta += stepsize
    if (n&8):
        newsample = max(-32768, hist[c] - delta)
    else:
        newsample = min(32767, hist[c] + delta)

    idx[c] = max(0, min(88, idx[c] + IMA_Step[n]))

    return newsample

for c in data:
    b = ord(c)
    hist[0] = decodeima(b / 16, 0)
    hist[1] = decodeima(b & 0xF, 1)

    outfile.write(pack("<hh", hist[0], hist[1]))
