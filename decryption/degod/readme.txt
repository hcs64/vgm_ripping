degod - decrypts ADX
	This version of degod comes with a set of 7 keys worked out from
	the games named. They may not be completely correct, but they produce
	good sounding audio.
	degod has the brute force tools that I've been using to find the keys
	built in.

	To get a list of keys:
	degod -k ?

	To decrypt a file with key 0:
	degod -k 0 file.adx

	Warning: Modifies the files it is given directly, by default. Change
	this behavior using the -o option to specify an output file.

	Run degod with no parameters for more usage details.

	xortest generates the sequence that is XORed with the ADX scales, given
	a key, stopping when it has repeated. This can be used to extrapolate
	a key for the start of the stream if one can be found for other parts
	of the stream. The encoder seems to be fairly careful and doesn't
	encrypt silent frames, as it would then be much easier to decrypt, and
	when these are at the start of a file the brute forcer gets started in
	a very wrong direction.

	I've included brute.txt, which describes the process I used to get the
	Senko no Ronde key. Please forward any keys you find to me so that I
	can include them in degod.

	degod is found at http://www.hcs64.com/in_cube.html

Version history:
0.0 - 10/19/07 - first version
0.1 - 10/22/07 - expanded, multiple keys and brute forcing
0.2 - 10/22/07 - Raiden III key
0.3 - 11/28/07 - Phantasy Start Universe: Ambition of the Illuminus key
0.4 - 12/23/07 - Senko no Ronde key, PSU:AOI key is also used in plain PSU
0.5 - 01/16/08 - NiGHTS: Journey of Dreams key
