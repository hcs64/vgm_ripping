all: ripping_mama ripping_mama.exe

ripping_mama: ripping_mama.c util.c util.h streamfile.c streamfile.h streamtypes.h

ripping_mama.exe: ripping_mama.c util.c util.h streamfile.c streamfile.h streamtypes.h
	i586-mingw32msvc-gcc ripping_mama.c util.c util.h streamfile.c streamfile.h streamtypes.h -o ripping_mama.exe
