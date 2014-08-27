gfo2png.exe: gfo2png.c
	i586-mingw32msvc-gcc -std=c99 gfo2png.c -o gfo2png.exe -Llibpng/lib/ -lpng12 -Ilibpng/include/libpng12/ -Lzlib/lib/ -lz -Izlib/include/ 
	i586-mingw32msvc-strip gfo2png.exe

clean:
	rm -f gfo2png.exe
