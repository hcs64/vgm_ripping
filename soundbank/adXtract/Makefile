BIN    = adXtract2.exe
CFLAGS = -W -Wall -pedantic -O3

.PHONY: all clean

all: $(BIN)

clean:
	rm -f *~ $(BIN)

$(BIN): adXtract2.c
	gcc $(CFLAGS) $< -o $@
	strip $(BIN)
