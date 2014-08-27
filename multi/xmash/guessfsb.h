#ifndef _GUESSFSB_H_INCLUDED
#define _GUESSFSB_H_INCLUDED

#include <stdint.h>

// should return 1 to check more keys, 0 to finish
typedef int good_key_callback_t(const uint8_t *, long file_size, void *);

int guess_fsb_keys(const uint8_t *infile, long file_size, good_key_callback_t *cb, void *cbv);

#endif // _GUESSFSB_H_INCLUDED
