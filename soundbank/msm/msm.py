#!/usr/bin/python

# 0.0
# export .dsp from MRODDR_Snd.msm (DDR Mario Mix)

from sys import argv
from struct import unpack, pack
from io import open

infile = open(argv[1], "rb")
infile.seek(0, 2)   # end
infile_size = infile.tell()
infile.seek(0)

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

infile.stack = []

def jsr(base = 0):
    newpos = read32()
    infile.stack.append(infile.tell())
    infile.seek(newpos + base)

def rts():
    infile.seek(infile.stack.pop())

# parse main header
assert(infile.read(4) == "GSND")

version = read32()
assert(version == 2)

assert(read32() == infile_size)
assert(read32() == 0)

chunk_offset = []
chunk_size = []
for a in range(7):
    chunk_offset.append(read32())
    chunk_size.append(read32())

# begin parsing chunk 2
for i in range(chunk_size[2]/0x20):
    infile.seek(chunk_offset[2]+i*0x20)

    group_id = read16()
    banks = read8()
    assert(read8() == 0)

    if (banks == 0): continue

    assert(banks == 1)

    bank_off = read32() + chunk_offset[5]
    bank_size = read32()
    data_off = read32() + chunk_offset[6]
    data_size = read32()

    print("group %02x: %d bank (%x %x)" % (group_id, banks, bank_off, data_off))

    infile.seek(bank_off)

    subchunk = [read32(),read32(),read32()]
    subchunk_size = read32()

    # subchunk 2 has most of the interesting stuff
    ssc2_offset = bank_off+subchunk[2]
    infile.seek(ssc2_offset)

    sample_idx = 0
    while 1:
        # 0: unknown ID, convenient sentinel since there is no count
        sample_id = read32()
        if sample_id == 0xFFFFFFFF: break

        assert(sample_id & 0xFFFF == 0)
        sample_id = sample_id >> 16

        outfile_name = "%04x_0_%04x.dsp" % (group_id, sample_id)
        print("%d: %x (%s)" % (sample_idx, sample_id, outfile_name))
        
        # 4: sample offset into chunk 6
        sample_offset = read32()

        # 8
        assert(read32() == 0)
        # C
        assert(read8() == 0x3C)
        # D
        assert(read8() == 0)
        # E sample rate
        sample_rate = read16()
        # 10 sample count
        sample_count = read32()
        # 14
        loop_start = read32()
        loop_end = read32()

        jsr(ssc2_offset)

        # sample parameters
        assert(read16() == 8)
        initial_ps = read8()
        loop_ps = read8()
        loop_hist1, loop_hist2 = unpack(">hh", infile.read(4))

        # coefs
        coefs = infile.read(0x20) #unpack(">"+("h"*16), infile.read(0x20))

        # output
        outfile = open(outfile_name, "wb")

        # build DSP header
        nibble_count = samples_to_nibbles(sample_count)
        loop_flag = 0
        if (loop_end != 0):
            loop_flag = 1

        outfile.write(pack(">IIIHHIII", sample_count, samples_to_nibbles(sample_count), sample_rate, loop_flag, 0, samples_to_nibbles(loop_start), samples_to_nibbles(loop_end), 0))

        outfile.write(coefs)

        outfile.write(pack(">HHHHHhh", 0, initial_ps, 0, 0, loop_ps, loop_hist1, loop_hist2))

        outfile.write("\0"*(0x60-0x4a))

        # sample data
        infile.seek(data_off+sample_offset)
        
        sample_data = infile.read(int((samples_to_nibbles(sample_count)+1)/2))
        assert(ord(sample_data[0]) == initial_ps)
        outfile.write(sample_data)

        outfile.close()
        outfile = 0

        rts()
        sample_idx = sample_idx + 1

