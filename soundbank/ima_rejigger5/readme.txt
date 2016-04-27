ima_rejigger5

decode Wwise RIFF/RIFX 0x2 uninterleaved MS IMA ADPCM

usage: ima_rejigger5 file.wem file.wav

These files are often called 1971021.wem or similar.

Unlike previous versions of ima_rejigger:
- this now supports both RIFF and RIFX in the same program
- the data is now decoded to PCM
- it doesn't write over the source file

There is no loop support, however.

---

Thanks to Zwagoth whose wwise_ima_adpcm gave me the final clue, that Wwise
uses one less sample per block than standard MS IMA:

MS IMA has the weird feature that every block, which is fairly small, has in
its header the first sample in full 16-bit PCM form, which allows for very
fast, accurate seeking.
The rest of the block is 4 byte chunks with 8 4-bit samples each, so the sample
count of a block is always 8x+1.

In Wwise IMA, the final decoded sample is thrown away, and in fact that last
nibble is always 0. Thus, sample count is only 8x. I assume AudioKinetic did
this because it lets them keep some buffer aligned to nice round numbers.

The upshot of which is that it is impossible to convert Wwise IMA ADPCM directly
to standard Microsoft IMA ADPCM; there are these extra samples hanging out which
throw the whole file off. There is actually a nice space in IMA's extra format
data for "samples per block", but I don't think many decoders care about that
(sox rejects anything without that extra +1 sample per block, libavformat
seemingly ignores it).

Whew.
-hcs
