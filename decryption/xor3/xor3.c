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
    printf("v0.0 2017-12-01\n");
    printf("usage: xor3 IN.MLX OUT.MLX\n");
    return -1;
  }
  FILE* infile = fopen(argv[1], "rb");
  FILE* outfile = fopen(argv[2], "wb");

  uint8_t bytes[16];
  fread(bytes, 1, 4, infile);
  uint32_t key = readuint32le(bytes);

  fseek(infile, 0, SEEK_SET);

  bool first_frame = true;
  while (!feof(infile) && fread(bytes, 1, 16, infile) == 16) {
    uint32_t next_frame_key;
    
    for (int i = 0; i < 4; i++) {
      writeuint32le(bytes + i * 4, readuint32le(bytes + i * 4) ^ key);
      key = key * 3 + 1;
      if (i == 0) {
        next_frame_key = key;
      }
    }

    key = next_frame_key;

    if (first_frame) {
      uint8_t zeroes[16] = {0};
      if (memcmp(bytes, zeroes, 16)) {
        printf("%s error: header not decrypted properly\n", argv[1]);
      }
    } else {
      fwrite(bytes, 1, 16, outfile);
    }
    first_frame = false;
  }

  fclose(infile);
  fclose(outfile);
}
