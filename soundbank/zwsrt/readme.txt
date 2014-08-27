zwsrt
Unpack the headers from Zack & Wiki .srt files and attach them to .ssd files. The combined file, with a .zwdsp extension, is then playable in in_cube 0.30 (and higher?).

zwsrt needs to know the .ssd file to use for each header in the .srt, so I've created zwlist which generates a batch file specifying all the command lines needed. Just throw everything (the .srt and .ssd files and zwsrt.exe)  into one directory and run zwlist.bat, 2000+ .zwdsp files will be created.


0.1 adds support for files from Monster Hunter 3.
