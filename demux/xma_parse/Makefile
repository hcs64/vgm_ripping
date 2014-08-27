CXXFLAGS=-ansi -pedantic -Wall -Weffc++ -Wextra -Wold-style-cast -O
CXX=i586-mingw32msvc-g++
CC=i586-mingw32msvc-g++
#CXX=g++
#CC=g++

xma_test: xma_test.o xma_parse.o

xma_test.o : xma_test.cpp xma_parse.h Bit_stream.h

xma_parse.o: xma_parse.cpp xma_parse.h Bit_stream.h

clean:
	rm -f xma_test xma_test.o xma_parse.o
