#include <stdio.h>
#include "util.h"
#include "error_stuff.h"
#include "parseriff.h"

/*
I refer to the structure described here: http://home.roadrunner.com/~jgglatt/tech/wave.htm

typedef struct {
  ID        chunkID;
  long      chunkSize;

  long      dwCuePoints;
  CuePoint  points[];
} CueChunk;

typedef struct {
  long    dwIdentifier;
  long    dwPosition;
  ID      fccChunk;
  long    dwChunkStart;
  long    dwBlockStart;
  long    dwSampleOffset;
} CuePoint;
*/

const uint32_t FMT_CHUNK_ID = UINT32_C(0x666d7420);
const uint32_t CUE_CHUNK_ID = UINT32_C(0x63756520);
const uint32_t DATA_CHUNK_ID = UINT32_C(0x64617461);

const long cuepoint_size = 0x18;

int main(int argc, char ** argv) {
    CHECK_ERROR(argc != 2, "usage: wavecue infile.wav");
    FILE * infile = fopen(argv[1], "rb");
    CHECK_ERRNO(infile == NULL, "open of input failed");

    long first_chunk, wave_end;
    check_riff_wave(infile, &first_chunk, &wave_end);

    struct chunk_info chunks[] =
    {
        { .id = FMT_CHUNK_ID }, // 0 fmt
        { .id = CUE_CHUNK_ID,   // 1 cue
          .optional = 1},
        { .id = DATA_CHUNK_ID },// 2 data
    };

    const int CUE_IDX = 1;

    CHECK_ERROR(find_chunks(infile, first_chunk, wave_end, chunks, sizeof(chunks)/sizeof(chunks[0]), 1), "RIFF parse failed");

    CHECK_ERROR(!chunks[CUE_IDX].found, "RIFF does not have a cue chunk");


    ///

    const int cue_count = get_32_le_seek(chunks[CUE_IDX].offset, infile);
    for (int i = 0; i < cue_count; i++)
    {
        const long cuepoint_offset = chunks[CUE_IDX].offset + 4 + i * cuepoint_size;
        const int id = get_32_le_seek(cuepoint_offset+0, infile);
        const long samples = get_32_le_seek(cuepoint_offset+4, infile);

        printf("%d: %ld\n", id, samples);
    }

    fclose(infile);

}

