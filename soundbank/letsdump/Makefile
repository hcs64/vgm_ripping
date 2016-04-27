letsdump: letsdump.c util.c util.h streamfile.c streamfile.h streamtypes.h

all: letsdump letsdump.exe

letsdump.exe: letsdump.c util.c util.h streamfile.c streamfile.h streamtypes.h
	i586-mingw32msvc-gcc letsdump.c util.c util.h streamfile.c streamfile.h streamtypes.h -o letsdump.exe
