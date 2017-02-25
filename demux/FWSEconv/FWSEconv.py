#!/usr/bin/python

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
def write32(x): outfile.write(pack("<I",x))
def write16(x): outfile.write(pack("<H",x))
def write16signed(x): outfile.write(pack("<h",x))

assert(infile.read(4) == "FWSE")
assert(read32() == 2)
file_len = read32()
header_len = read32()
channels = read32()
assert(channels == 1)
sample_count = read32()
sample_rate = read32()

infile.seek(0, 2)   # end
real_file_len = min(infile.tell(), file_len)
infile.seek(header_len)

data = infile.read(real_file_len - header_len)
payload_size = len(data)
sample_count = payload_size*2
infile.close()

# write RIFF header
outfile.write("RIFF")
write32(payload_size*4+0x24)  # total size
outfile.write("WAVEfmt ")

write32(0x10)   # fmt chunk size
write16(0x1)   # ima codec id
write16(channels)
write32(sample_rate)
write32(sample_rate*channels*2)
write16(2)  # block align
write16(16*channels)  # bits per sample

outfile.write("data")
write32(payload_size*4)

hist = 0
idx = 0

def convnibble(n):
    if n & 8:
        return n & 7
    else:
        return n^0xF

def decodeima(n):
    global idx, hist
    stepsize = IMA_Scale[idx]

    delta = stepsize >> 3
    if (n&1): delta += stepsize >> 2
    if (n&2): delta += stepsize >> 1
    if (n&4): delta += stepsize
    if (n&8):
        newsample = max(-32768, hist - delta)
    else:
        newsample = min(32767, hist + delta)

    idx = max(0, min(88, idx + IMA_Step[n]))

    return newsample

for i in range(len(data)):
    b_in = ord(data[i])

    n = convnibble((b_in & 0xF0)/16)
    hist = decodeima(n)
    write16signed(hist)

    n = convnibble(b_in & 0xF);
    hist = decodeima(n)
    write16signed(hist)

