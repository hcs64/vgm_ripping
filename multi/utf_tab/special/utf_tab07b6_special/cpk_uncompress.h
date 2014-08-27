#ifndef _CPK_UNCOMPRESS_H_INCLUDED
#define _CPK_UNCOMPRESS_H_INCLUDED

#include <stdio.h>
#include "util.h"

long uncompress(reader_t *infile, long offset, long size, FILE *outfile);

#endif
