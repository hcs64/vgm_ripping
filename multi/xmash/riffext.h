#ifndef _RIFFEXT_H
#define _RIFFEXT_H

#include <stdint.h>
#include "fsbext.h"
#include "riffext.h"

// returns 0 if all was ok, 1 otherwise
int try_xma_riff(const uint8_t *infile, long file_size, subfile_callback_t *cb, void *cbv);

#endif // _RIFFEXT_H
