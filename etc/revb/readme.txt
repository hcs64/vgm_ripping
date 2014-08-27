Revolution B (revb) is a tool for manipulating the RSTM files (.brstm) used by
many games for the Nintendo Wii.

It can:
- build .brstm from one or more standard .dsp files
- extract .dsp from .brstm
- examine .brstm format details (essentially extract with no output files)

*** DSP

Standard .dsp is the format used by the old dspadpcm tools for GameCube, which
encodes a source .wav file to .dsp. Note though that standard .dsp is strictly
a mono format, so you will need a separate file for each channel. revb combines
these multiple .dsp files into a single .brstm.

*** BRSTM VARIANTS

revb was made by examining .brstms from a variety of Wii games. As it is based
on reverse engineering there is a decent chance that I have missed details of
the format. There are a few variations on what I consider the standard format
(that used in Super Smash Bros Brawl); revb can still generate these but it
requires extra options on the command line. When extracting or examining, revb
will inform you of the options you would need to use to generate a .brstm with
the same features. It also performs a good many consistency checks. If you
intend to use it to replace music in a game, it is recommended that you extract
an exisiting file to check that revb understands the format, and to find what
options might be needed to generate a replacement.

*** USAGE

Usage is explained by running the executable with no arguments. In case
you are incapable of seeing that for some reason, it says:

Revolution B
Version 0.4 (built Jul 23 2009)

Build .brstm files from mono .dsp (or extract)
examine usage:
    revb.exe --examine source.brstm
build usage:
    revb.exe --build dest.brstm source.dsp [sourceR.dsp ...] [options]
extract usage:
    revb.exe --extract source.brstm dest.dsp [destR.dsp ...]
build options:
  --second-chunk-extra
  --alternate-adpc-count

In case this isn't clear, the build usage means that source.dsp is the first
channel (the left channel of a stereo stream or the only channel of mono),
and sourceR.dsp is the second channel (the right channel for a stereo stream).
If you only want one channel only specify one source .dsp.

*** LOOPING

A limitation of the .brstm playback code appears to be that it can only loop
to the beginning of a block. revb will use the loop information given in the
.dsp headers, but if the loop start is not properly aligned it will output a
message reporting the issue. This can be solved by padding the beginning of
the file with some number of silent samples, and revb will report how many are
needed.

*** LIMITATIONS

While there is support for extracting up to 8 channels, revb currently only
supports building 2 channel .brstms.

Only .brstms bearing DSP format audio are supported. Theoretically 16 or 8-bit
PCM can be used but I haven't seen any such files yet.

A notable missing feature is that revb does not attempt to fill the ADPC table
with valid values. I think this table is used for seeking, but as it isn't
used for normal looping and I don't know exactly what values are supposed to
go here, I leave it filled with zeroes.

*** SOURCE CODE
I've started building revb.exe so that it is both a Windows executable
and a Zip archive containing the source code. I've seen this trick used
somewhere and it seemed like a cool idea to ensure that the source gets out
there.

Enjoy!
-hcs (http://here.is/halleyscomet)
