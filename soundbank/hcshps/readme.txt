hcshps 0.0

NOTE: This code currently does NOTHING PRACTICAL, it is just a testbed for
theories about .hps structure.
As such, it actually fails a lot of genuine files. I was working from the
assumption that the decode state at loop end would match that at loop start,
however this turns out to not be the case. Thus the tests from line 440 to 443
shoud probably be skipped if loop_pass is true.
Ultimately this was supposed to be something like Revolution B for brstms.
Nominally the program supports --examine and --extract modes, though neither
print any useful info, and --extract only writes DSP headers.
