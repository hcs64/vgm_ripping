guessadx 0.4
by hcs
http://here.is/halleyscomet

guessadx computes all possible encryption keys for an ADX file.
It does not actually do decryption, that is the function of another utility,
degod. Also, many known keys are built into the vgmstream decoder, which can
decrypt on the fly.
All keys can be computed in about 20 minutes on a fast machine (tests were done 
on a 2.4GHz Intel Core 2 Duo).
degod can be configured to search only a portion of the search space, so it can
be used in parallel across several CPUs, hardware threads, or systems.

* A background on ADX encryption

An ADX stream is composed of 18 byte frames, each containing a two byte header,
known as the scale. This is a single 16-bit big endian value which is multiplied
with every sample in the frame (technically the scale +1 is used). The remaining
16 bytes of the frame each represent two samples.

ADX supports a simple encryption, wherein the sequence of values from a linear
congruential generator (LCG) is XORed with the scale of each frame.
To decrypt we simply compute the output of the LCG and XOR it with the encrypted
scale to produce the original scale value. This is a basic property of XOR,
if S XOR R = C then C XOR R = S (S is the original scale, R is the random number
output from the LCG, and C is the encrypted scale), so the same operation is
used to encrypt and decrypt.
Another important property of XOR is that A XOR 0 = A, which will come up later.

An LCG is a very simple pseudo-random number generator, which produces a
series of seemingly random numbers. To compute the next value in the sequence
(rand_1) from the current value (rand_0) we use the following equation:

    rand_1 = (rand_0 * multiplier + increment) MOD modulus

Where multiplier, increment, and modulus are parameters. With those parameters
and an initial random value (which we'll call the start value) we can compute
the entire sequence of numbers.

In the case of ADX encryption, the modulus used in 32768 (8000 in hexadecimal).
The other values (start, multiplier, and increment) form the encryption key, if
we know these we can easily decrypt the ADX. The trick, then, is determining the
key. As far as I have seen every ADX file in a particular game uses the same
key, though there is no reason why this must be universally true.

An additional wrinkle: The ADX encrypter does not perform encryption on a silent
frame, which is a frame consisting entirely of zeroes. This is because if the
samples are all zero, the scale will also be zero (this is a property of the ADX
encoder but it is not necessary, as we will see). Any value XORed with the zero
scale will be that same value (due to the earlier mentioned property of XOR),
which would allow precise recovery of a single value of the random stream;
this would allow the key to be determined much easier. So the scale value is
left at zero rather than give away part of the LCG stream. When the decoder is
decrypting the frame it will XOR the zero scale with the output of the LCG,
which will yield a random scale value, but this is unimportant as the random
scale will be multiplied by the zero samples within the frame and thus the
result will always be zero.

While the scale is 16 bits, only 15 of those are actually usable; the high bit
is used as a signal of the end of the stream. The LCG that ADX encryption uses
produces random values filling the entire 15 bit range (0 to 32767). However,
valid scale values are only 13 bits (0 to 8191). This means that the three high
bits of a scale value are always zero. This also means that in an encrypted ADX,
two bits of the range are equal to two bits of the LCG output, since they are
XORed with zero bits from the scale and are therefore unchanged.

* How guessadx works

guessadx takes advantage of this property, that we know exactly two bits of the
LCG output. Using the two known bits of the start value, we guess each of the
8192 possible values for the whole start value. For each of these, we guess each
of the 3512 possible prime multiplier values. For each of these, we guess the
3512 possible prime increments. We then compute the LCG values and check them
against the two bits of all the scales (actually only up to 32768, as the LCG
will repeat after at most that many; also we stop as soon as we hit a
nonmatching value). We end up checking 8192*3512*3512 = 1.01e11 keys. That's
an awful lot, but compare it to the 2.81e14 keys in the whole keyspace.

This method is a huge improvement over the old brute force method used in degod.
It is still a brute force method, but it is precise (generates all possible
keys) and much faster.

Another bit of detail is that the checker needs a string of encrypted values to
work with. If there are any silent frames, which are unencrypted, we won't be
able to check with them, but the LCG still computes a value for them so we
can't simply skip them. guessadx searches for a long series of consecutive
encrypted frames to work with. However, this means that the start value it
initially finds is not actually the start value for the whole file. To find that
we guess every possible start value (32768 of 'em), and check that they would
produce the values we see at the beginning of the file, culminating in the fake
start value that we were using earlier. As this only has to be done once for
every found key, and found keys are very rare, it doesn't slow down the overall
computation at all.

* guessadx output

The primary output of guessadx is on standard output. Whenever it finds a key
that works it outputs it in this format:

    -s 49e3 -m 4a57 -a 4091 (error 13793869)

The first three values are the start, multiplier, and increment of the key, in
hexadecimal. The -s, -m and -a are command line switches to degod. You can copy
that section verbatim onto a degod command line, like so:

    degod -s 49e3 -m 4a57 -a 4091 song.adx

The last part (error) is the total of the decrypted scale values in the range
that was checked; this can in principle be used to judge between multiple keys
(lower is better, but very large values can still be correct), though in
practice it usually isn't useful.

guessadx outputs some status information on standard error so you can tell how
long it is going to take. This is formatted as follows:

   9e0  30%      121 minutes elapsed      272 minutes left (maybe)

The first value is the current guess for the low bits of start, in hexadecimal.
The second value is the percentage of the total computation that is complete.
The third value is the time since the guessing portion of the program began.
The fourth value is an estimate of how much time remains, assuming that the
program continues checking keys at the same rate it has been all along.

* Parallel processing
guessadx has support for a primitive parallel processing wherein it splits up
the guessed first scale into regions and assigns one to each instance of
guessadx. If you wanted to run three instances of guessadx on a particular
file, it would be invoked like so:

    guessadx blah.adx 0 3
    guessadx blah.adx 1 3
    guessadx blah.adx 2 3

The first number is the node id, and the second is the total number of nodes.

* Issues

I do not know for certain if the multiplier and increment must always be prime,
but it has always held so far. If we don't discover any key for a file this
might be the first thing to check.

When multiple keys are generated, there is no easy way to choose among them.
There are certain LCGs for which there exist other LCGs that differ only in the
low bits. These bits are lost in the randomness inherent in the low bits of the
ADX scales. This difference may be audible if one listens carefully; using
incorrect keys usually results in a bit of noise. guessadx can't make this
determination for you, so it offers up all keys it finds. It may be possible to
automatically detect this using statistical methods, as the low bits of an LCG
with a power of two modulus cycle much faster than the high bits.

I suspect that there must be a much faster way to compute LCG parameters given a
few bits of the output, but the math eludes me.

* Notes

I'd like to note that this method could have been defeated if the LCG had only
13-bit random values. This would not have rendered the files as completely
unlistenable as the 15-bit randomness, but it would still have sounded like
noise. We would have been stuck with far less efficient keyfinding methods if
this was the case. However this would result in the encrypted audio being
recognizable as the original song, just very noisy, which would reveal this as
the lousy encryption it is.

The prime multiplier and increment are, I assume, an attempt to ensure that the
LCG cycles through all 32768 values before repeating, however this is not
ensured by only insisting on primes, and so we often have keys with shorter
periods. To quote (but typographically mutilate) Knuth:
The linear congruential sequence defined by m, a, c, and X_0 has period length
m if and only if
i)   c is relatively prime to m;
ii)  b = a - 1 is a multiple of p, for every prime p dividing m;
iii) b is a multiple of 4, if m is a multiple of 4.
[Donald E. Knuth, The Art of Computer Programming Vol. 2 3rd ed. p17]
X_0 is our "start", the modulus m is 32768, c is the "increment" and a is the
"multiplier".
With c prime, i is satisfied.
The only prime factor of m is 2, a is prime and therefore odd and a-1 is even,
so ii is satisfied.
However, as m is divisible by 4, we require further that a-1 be divisible by
4 as well, which is not guaranteed, so iii will often fail.
If this was required, it would further narrow the search space, though not by
much.

-hcs
