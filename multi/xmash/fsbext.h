#ifndef _FSBEXT_H
#define _FSBEXT_H

#include <stdint.h>

// should return 0 if subfile was ok, 1 otherwise
typedef int subfile_callback_t(const uint8_t *, long file_size, int streams, int * stream_channels, long samples, long srate, long block_size, long loop_start, long loop_end, const char *stream_name, void *);

// returns 0 if all was ok, 1 otherwise
int try_multistream_fsb(const uint8_t *infile, long file_size, subfile_callback_t *cb, void *cbv);

#endif // _FSBEXT_H
