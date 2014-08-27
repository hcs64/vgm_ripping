fsb_mpeg creates .mp3 files from multi-stream .fsb files.
You can use it like this:

fsb_mpeg file.fsb

In some cases you will need to specify a different padding size,
for instance for Bioshock 2:
fsb_mpeg file.fsb -p 4

This is because the MPEG frames are rounded up to 4 bytes, rather than
the 16 bytes which is the case elsewhere.

As of 0.7 this is treated as a maximum padding, again Bioshock 2 is
irritatingly different and the padding is sometimes less. The parser will search
for a sync back through the padding if it doesn't find one. This mechanism is
only enabled if -p is specified.

0.11 adds support for padding between stream with the -b switch. For instance,
Apache Armed Assault needs:
fsb_mpeg file.fsb -p 16 -b 32
