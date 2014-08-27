all: brsar_unpack brsar_unpack.exe

brsar_unpack: brsar_unpack.c util.c util.h streamfile.c streamfile.h streamtypes.h gcdsp_decoder.c gcdsp_decoder.h

brsar_unpack.exe: brsar_unpack.c util.c util.h streamfile.c streamfile.h streamtypes.h gcdsp_decoder.c gcdsp_decoder.h
	i586-mingw32msvc-gcc brsar_unpack.c util.c util.h streamfile.c streamfile.h streamtypes.h gcdsp_decoder.c gcdsp_decoder.h -o brsar_unpack.exe
