#!/usr/bin/python
#

from io import open
from sys import argv
from struct import pack
from math import trunc

infile_name = argv[1]
outfile_name = argv[2]
infile = open(infile_name, 'rb')
outfile = open(outfile_name, 'wb')

val_table = (1, -1, 2, -2, 3, -3, 5, -5, 7, -7, 9, -9, 11, -11, 13, -13)

lpc_hist1 = 0
lpc_hist2 = 0
while 1:
    b = infile.read(1)
    if len(b) < 1:
        break
    num = ord(b[0])
    high_nibble = (num >> 4) & 0xF
    low_nibble = num & 0xF

    #for nibble in (high_nibble, low_nibble):
    for nibble in (low_nibble, high_nibble):
        sample = trunc((lpc_hist1 * 4025 + lpc_hist2 * -990)/4096.0) + val_table[nibble]*256

        if sample > 32767:
            sample = 32767
        elif sample < -32768:
            sample = -32768
        lpc_hist2 = lpc_hist1
        lpc_hist1 = sample

        outfile.write(pack("<h",sample))
