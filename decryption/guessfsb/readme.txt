guessfsb 0.3 - Encryption key guessing for FSB3/FSB4 files

By using elements of the FSB file structure, guessfsb can check a possible
encryption key. It has two methods of determining possible keys to check:

1. Assume the file ends with some constant padding byte
By looking for a repetition in the encrypted data at the end of the file,
compute a key by assuming that the underlying data is a single byte repeated to
pad out the file. Check that the header decrypted by this key is consistent.
This works for keys up to half the size of such padding.

2. Assume that the file contains a valid header for some number of streams
Using the file size, construct possible headers (the first 16 bytes can be
guessed fairly quickly for files with few streams), and check that the key
needed to generate that header decrypts the stream header table properly. This
can work with up to 15 byte keys, but it does not work for multiple FSBs packed
into the same file, and it becomes exponentially slower for large numbers of
streams. It is also somewhat prone to false positives, particularly if only one
stream is present.

***

Usage:
guessfsb infile

Example Input:
guessfsb Music_P1_100.fsb

Example Output:

guessfsb 0.1
Trying tail padding...
Possible key 1: 44 46 6d 33 74 34 6c 46 54 57 : "DFm3t4lFTW"
Trying headers with stream counts from 1 to 109267...
trying 20 streams
trying 40 streams
...

Here DFm3t4lFTW is the key, in text form. The digits before it are the
hexadecimal form, which can be useful if the text contains nonprinting
characters.
Note that the program will continue to run. You can stop it with Ctrl-C
if it has printed a key; it will take decades for it to run through all
possibilites and lower stream counts are more likely. I don't stop it after
printing one key because it is possible that several keys result in consistent
headers, but they will likely be output in quick succession.
Also note that while the two methods may arrive at the same key, a particular
key (and synonymous strings) will only be output once.

*** decfsb

I've provided decfsb as a simple decrypter. 

Usage:
decfsb infile outfile [textkey | -x 11 22 AA BB ...]

Example with text key:
decfsb Music_P1_100.fsb Music.fsb "DFm3t4lFTW" 

Example with hexadecimal key:
decfsb Music_P1_100.fsb Music.out.fsb -x 44 46 6d 33 74 34 6c 46 54 57

Both of these examples will do the same thing, namely decrypt Music_P1_100.fsb
to Music.out.fsb. The hexadecimal support is for use with keys that contain
nonprinting characers.

Credits:
I use the SwapBitBytes function from Invo's GHIII FSB Decryptor v1.0.

-hcs
