voxhound 0.2

For replacing audio in Metal Gear Solid PC VOX files.

---

Run extract.bat on the .VOX file(s). This will extract the audio into a .VAG
file and decode it to PCM .WAV with MFAudio.

After you've modified the .WAVs (without moving them, run replace.bat on the
original .VOX files. This will encode the audio back to .VAG and replace it in
the .VOX. This can be repeated without redoing the initial extraction.

The batch files will delete the intermediate .VAG files.

This will directly modify the original .VOX files, so be sure to keep a clean
copy in case something goes wrong.

---

I haven't tested this at all with the game yet, just with a few sample files.

-hcs

--------------------------------------------------------------------------------

History:
0.2 - 2017-02-24 - add proper parenthesis in frame validation logic

0.1 - 2017-02-24 - allow 04 flags (when MFAudio is set to "Use Extended
Flagging", the default, it sets this on the first frame)

0.0 - 2017-02-24