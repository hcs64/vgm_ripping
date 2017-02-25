Email dated 2012-01-27, not sure what game this came from

---

Well, the good news is this is definitely not ogg vorbis.
Looking at the frequency distribution in the body of the file, we see this:
first nibble
00: 98
01: 86
02: 241
03: 372
04: 606
05: 816
06: 941
07: 770
08: 1088
09: 920
10: 819
11: 618
12: 369
13: 208
14: 109
15: 102
second nibble
00: 84
01: 106
02: 202
03: 395
04: 611
05: 839
06: 956
07: 765
08: 1124
09: 924
10: 778
11: 584
12: 381
13: 202
14: 118
15: 94

The symmetrical shape between nibbles suggests a 4-bit coding of some
sort, usually that means ADPCM.
The exact encoding is a bit odd. The most common 4-bit ADPCM is called
IMA or DVI, but this isn't it exactly; there we expect large values at
0, tapering off to 7, and then jumping again at 8, a bit higher at 9
(about the same as 1), and tapering off again.  This is because the
high bit of the nibble is the sign, there is both a +0 (0) and a -0
(8, which only occurs when the rounding falls a certain way, so it
occurs less often than 0, 1, or -1).

This frequency distribution is similarly patterned but the order seems
wrong, it suggests to me that 8 is 0 delta, and 7 is -0.  I tried
inverting it so that 8-15 was mapped directly to 0-7, and 0-7 was
inverted to 15-8. This then looked like IMA, and it decoded fine with
sox (strip off the header and run "sox -t ima infile out.wav").  I
don't like having to rely on sox if I can avoid it, so I made a
complete Python script to do the decode, which I have attached, it
works with Python 2.6 and probably any version < 3.0.  Run it like:
python FWSEconv.py infile out.wav
