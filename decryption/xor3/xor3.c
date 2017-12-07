#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

uint32_t readuint32le(uint8_t arr[4]) {
  uint32_t v = arr[3];
  v = v * 0x100 + arr[2];
  v = v * 0x100 + arr[1];
  v = v * 0x100 + arr[0];

  return v;
}

uint32_t writeuint32le(uint8_t arr[4], uint32_t v) {
  arr[0] = v & 255;
  v >>= 8;
  arr[1] = v & 255;
  v >>= 8;
  arr[2] = v & 255;
  v >>= 8;
  arr[3] = v & 255;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    printf("MLX XOR3 decryptor for Valkyria Chronicles 2\n");
    printf("v0.1 2017-12-06\n");
    printf("usage: xor3 IN.MLX OUT.MLX\n");
    return -1;
  }
  FILE* infile = fopen(argv[1], "rb");
  FILE* outfile = fopen(argv[2], "w+b");

  fseek(infile, 0, SEEK_END);
  long bytes_left = ftell(infile) - 0x10;

  fseek(infile, 0, SEEK_SET);

  uint8_t bytes[128];
  fread(bytes, 1, 16, infile);
  uint32_t key = readuint32le(bytes);
  uint32_t next_line_key;
  key = key * 3 + 1;
  next_line_key = key;
  uint32_t byte_key = key;

  for (int i = 4; i < 16; i+= 4) {
    if ((readuint32le(bytes + i) ^ key) != 0) {
      printf("%s: error decrypting header @ %d\n", argv[1], i);
      exit(-1);
    }
    key = key * 3 + 1;
  }

  int bytes_per_frame = 128;
  while (bytes_left && !feof(infile) &&
      fread(bytes, 1, bytes_per_frame, infile) == bytes_per_frame) {

    if (bytes_per_frame == 1) {
      bytes[0] ^= key;
      key = key * 3 + 1;
    } else {
      for (int j = 0; j < 8; j++) {
        key = next_line_key;
        for (int i = 0; i < 4; i++) {
          writeuint32le(bytes + j * 16 + i * 4,
              readuint32le(bytes + j * 16 + i * 4) ^ key);
          key = key * 3 + 1;
          if (i == 0) {
            next_line_key = key;
          }
        }
      }
    }

    fwrite(bytes, 1, bytes_per_frame, outfile);
    bytes_left -= bytes_per_frame;

    if (bytes_per_frame > 1 && bytes_left < 0x80) {
      bytes_per_frame = 1;
      key = byte_key;
    }
  }

  fclose(infile);

  fseek(outfile, -0x10, SEEK_CUR);
  fread(bytes, 1, 0x10, outfile);

  uint8_t EOFC[4] = "EOFC";
  if (memcmp(bytes, EOFC, 4)) {
    printf("%s: didn't find final EOFC\n", argv[1]);
  }
  fclose(outfile);
}
