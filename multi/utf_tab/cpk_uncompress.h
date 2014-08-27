#ifndef _CPK_UNCOMPRESS_H_INCLUDED
#define _CPK_UNCOMPRESS_H_INCLUDED

#include <stdio.h>

long uncompress(FILE *infile, long offset, long size, FILE *outfile);

#endif
