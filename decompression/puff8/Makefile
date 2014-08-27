CFLAGS=-std=c99 -pedantic -Wall -ggdb

all: puff8

puff8: puff8.o util.o

puff8.o: puff8.c error_stuff.h util.h

util.o: util.c error_stuff.h util.h

clean:
	rm -f puff8 puff8.o util.o
