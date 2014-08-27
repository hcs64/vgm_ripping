CFLAGS=-std=c99 -pedantic -Wall -O2
#CFLAGS=-std=c99 -pedantic -Wall -ggdb -DDEBUG

all: cpk_unpack utf_view csb_extract usm_deinterleave cpk_crypt

csb_extract: csb_extract.o utf_tab.o util.o

csb_extract.o: csb_extract.c utf_tab.h error_stuff.h util.h

cpk_unpack: cpk_unpack.o cpk_uncompress.o utf_tab.o util.o

cpk_unpack.o: cpk_unpack.c utf_tab.h cpk_uncompress.h error_stuff.h util.h

cpk_uncompress.o: cpk_uncompress.c error_stuff.h util.h

cpk_crypt: cpk_crypt.o util.o

cpk_crypt.o: cpk_crypt.c utf_tab.h error_stuff.h util.h

usm_deinterleave: usm_deinterleave.o utf_tab.o util.o

usm_deinterleave.o: usm_deinterleave.c utf_tab.h error_stuff.h util.h

utf_view: utf_view.o utf_tab.o util.o

utf_view.o: utf_view.c utf_tab.h error_stuff.h util.h

utf_tab.o: utf_tab.c utf_tab.h error_stuff.h util.h

util.o: util.c error_stuff.h util.h

clean:
	rm -f csb_extract cpk_unpack cpk_crypt usm_deinterleave utf_view csb_extract.o cpk_unpack.o cpk_uncompress.o cpk_crypt.o usm_deinterleave.o utf_view.o utf_tab.o util.o 
