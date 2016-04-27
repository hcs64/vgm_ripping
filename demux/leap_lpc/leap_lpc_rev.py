#!/usr/bin/python
#

from io import open
from sys import argv
from struct import unpack, pack
from math import trunc

infile_name = argv[1]
outfile_name = argv[2]
infile = open(infile_name, 'rb')
outfile = open(outfile_name, 'wb')

val_table = (1, -1, 2, -2, 3, -3, 5, -5, 7, -7, 9, -9, 11, -11, 13, -13)

lpc_hist1 = 0
lpc_hist2 = 0

nc = 0
outbyte = 0
while 1:
    b = infile.read(2)
    if len(b) < 2:
        break
    sample = unpack("<h", b)[0]

    prediction = trunc((lpc_hist1 * 4025 + lpc_hist2 * -990)/4096.0)

    min_err = abs(sample - prediction)
    min_err_idx = 0
    for i in range(1,15):
        new_err = abs(sample - (prediction + val_table[i]*256))
        if new_err <  min_err:
            min_err = new_err
            min_err_idx = i

    lpc_hist2 = lpc_hist1
    lpc_hist1 = prediction + val_table[min_err_idx]*256

    if (nc == 0):
        outbyte = min_err_idx
        nc = 1
    else:
        outbyte += min_err_idx * 16
        outfile.write(pack("B",outbyte))
        nc = 0
