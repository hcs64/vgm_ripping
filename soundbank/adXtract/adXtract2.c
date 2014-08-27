#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#ifdef MSVC /* defined in project settings; not by default */
#  include <windows.h>
typedef DWORD uint32_t;
typedef WORD uint16_t;
#else
#  include <stdint.h>
#endif

/* adXtract 0.0, by hcs
 * adXtract 0.1 (25-apr-2007), by ak, based on hcs's code
 * adXtract 0.2 (25-nov-2007), by hcs:
 *    - fix reading of terminal frame, and use that as an additional check
 *    - support for encrypted ADXs
 */

#define FIXED_HDR_SIZE   20
#define BUFFER_SIZE      (128*1024) /* must be >= FIXED_HDR_SIZE!!! */
#define COPY_BUFFER_SIZE (64*1024)
/* ^^^ if you run out of stack space lower these or use malloc() ^^^ */

typedef uint32_t adx32_t; /* this better be 4 bytes long! */
typedef uint16_t adx16_t; /* this better be 2 bytes long! */

/* get 16/32 bit big endian values */

adx16_t get16bit (const unsigned char *p) {
  return (p[0] << 8) | p[1];
}

adx32_t get32bit (const unsigned char *p) {
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* returns 0 on -failure-, 1 on success. */
int adxtract (char *filename) {

  adx32_t type03 = *(adx32_t *)"\x01\xF4\x03\x00";
  adx32_t type03enc = *(adx32_t *)"\x01\xF4\x03\x08";
  adx32_t type04 = *(adx32_t *)"\x01\xF4\x04\x00";
  adx32_t type04enc = *(adx32_t *)"\x01\xF4\x04\x08";

  unsigned char buf[BUFFER_SIZE + FIXED_HDR_SIZE], *hdrptr;
  unsigned char outbuf[COPY_BUFFER_SIZE];
  FILE *infile, *outfile;
  char *t, *namebase, *fname;
  int success = 1, adxcount = 0, newbuf = 1;
  long offset, buflen = 0, fileoffset = -FIXED_HDR_SIZE, outbuflen, remaining;

  if (!(infile = fopen(filename, "rb"))) {
	perror(filename);
	return 1;
  }

  printf("%s...\n", filename);

  /* generate namebase; strip path and extension */
  if ((t = strrchr(filename, '\\')) || (t = strrchr(filename, '/')))
	namebase = strdup(t + 1);
  else
	namebase = strdup(filename);
  if ((t = strrchr(namebase, '.')) != NULL)
	*t = 0;
  fname = (char *)malloc(strlen(namebase) + 4/*num*/ + 4/*.adx*/ + 1);

  /*
 
  start of ADX data:
  
  80 ?? NN NN   - NN=offset to just after CRI sig from current pos
  ?? ?? ?? NC   - NC=channel count
  ?? ?? ?? ??
  NS NS NS NS   - NS=sample count
  AD AD AD AD   - AD=ADX sig (01F40300 or 01F40400)
  ...... then 6 bytes before NN:
  28 63 29 43 52 49  - CRI sig "(c)CRI"
  
  ADX file is (NN+4+6+NC*((NS+0x1F)/0x20)*0x12) bytes starting at the
  0x80 above.
    
   */

  while (!feof(infile)) {

	if (newbuf) {
	  memset(buf, 0, sizeof(buf));
	  newbuf = 0;
	}

	/* fileoffset is the file offset of the start of the buffer,
	 * initially it's FIXED_HDR_SIZE before the start of the file.
	 * FIXED_HDR_SIZE included so we don't miss ADX data at the
	 * edges of the read buffer. buf is initialized to 0's first
	 * so that we won't make a mistake thinking that the first
	 * FIXED_HDR_SIZE bytes of the uninitialized buffer is header
	 * data the first time through. */

	memcpy(buf, buf + BUFFER_SIZE, FIXED_HDR_SIZE);
	if ((buflen = fread(buf + FIXED_HDR_SIZE, 1, BUFFER_SIZE, infile)) <= 0)
	  break;

	for (offset = 0; offset < BUFFER_SIZE; ++ offset) {

	  adx16_t crioff;
	  adx32_t adxsig, adxns;
	  unsigned char crisigbuf[6], *crisig, adxnc;
	  long adxsize, adxoff, hdroffset;
	 
	  hdrptr = buf + offset;

	  /* check for ADX header sig */
	  adxsig = *(adx32_t *)(hdrptr + 16);
	  if (adxsig != type03 && adxsig != type03enc && adxsig != type04 && adxsig != type04enc)
		continue;

	  /* check for start of ADX */
	  if (hdrptr[0] != 0x80)
		continue;

	  /* check for CRI sig -- if we don't already have this in the
	   * buffer then read it from the file; overlapping reads will
	   * not kill us at this point. */
	  crioff = get16bit(hdrptr + 2);
	  crisig = hdrptr + 4 + crioff - 6;
	  if (crisig < buf || crisig >= (buf + buflen)) {
		/* it's not in buffer so read from file */
		long oldoff = ftell(infile);
		if (fseek(infile, fileoffset + offset + 4 + crioff - 6, SEEK_SET) ||
			fread(crisigbuf, 6, 1, infile) != 1) {
		  fseek(infile, oldoff, SEEK_SET);
		  continue;
		}
		fseek(infile, oldoff, SEEK_SET);
		crisig = crisigbuf;
	  }
	  if (memcmp(crisig, "(c)CRI", 6))
		continue;

	  /* number of samples and number of channels needed for size. */
	  adxnc = hdrptr[7];
	  adxns = get32bit(hdrptr + 12);
	  adxsize = crioff + 4 + adxnc * ((adxns + 0x1F) / 0x20) * 0x12 + 0x12;
	  adxoff = fileoffset + offset;

	  /* do this before modifying adxsize below. will skip the current adx
	   * data and start searching after; may jump out of this loop if there
	   * is no data left in the buffer (adxsize > remaining bytes). */
	  hdroffset = offset;
	  offset += adxsize - 1;

	  /* create ADX file */
	  if (adxcount > 65535) { /* anything > overflows fname */
		printf("too many adx files found... this is insane!\n");
		continue;
	  }
	  sprintf(fname, "%s%04x.ADX", namebase, adxcount);
	  if (!(outfile = fopen(fname, "wb"))) {
		perror(fname);
		success = 0;
		continue;
	  }

	  printf("%s\toffset=%08lx\tNCH=%d\tnrsamples=%-7d\tsize=%ld\n",fname,
			 (unsigned long)adxoff,adxnc,adxns,adxsize);

	  /* copy adx; first the parts that have already been read. */
	  outbuflen = adxsize;
	  remaining = (BUFFER_SIZE + FIXED_HDR_SIZE) - hdroffset;
	  if (outbuflen > remaining)
		outbuflen = remaining;
	  fwrite(hdrptr, outbuflen, 1, outfile);
	  adxsize -= outbuflen;

	  /* now write the rest, if there is any, reading new data from input
	   * file. */
	  while (adxsize) {
		outbuflen = (adxsize > COPY_BUFFER_SIZE) ? COPY_BUFFER_SIZE : adxsize;
		fread(outbuf, outbuflen, 1, infile);
		fwrite(outbuf, outbuflen, 1, outfile);
		adxsize -= outbuflen;
		newbuf = 1; /* we'll need to refill the output buffer then */
	  }

	  fclose(outfile);

      /* as an additional check, see if the terminal frame of the ADX looks valid */
      /* this is effective at pointing out incomplete extractions from SFDs */
      /* it is, however, not necessary for decoding, so I don't want to reject on
       * account of this */
      {
          FILE * checkfile;
          unsigned char buf[0x12];
          if (!(checkfile = fopen(fname, "rb"))) {
              perror(fname);
              success = 0;
              continue;
          }
          fseek(checkfile,-0x12,SEEK_END);
          fread(buf,0x12,1,checkfile);
          if (buf[0]!=0x80 || buf[1]!=0x01) {
              printf("*************%s is probably not complete\n",fname);
          }
          fclose(checkfile);
      }

	  ++ adxcount;

	}

	if (newbuf)
	  /* this will be the new start of the new buffer since adx data was just
	   * copied and the current buffer is exhausted. */
	  fileoffset = ftell(infile) - FIXED_HDR_SIZE;
    else
	  /* here we just keep on truckin */
	  fileoffset += buflen;

	fflush(stdout);

  }

  free(fname);
  free(namebase);
  fclose(infile);
  return success;

}


int main (int argc, char **argv) {

  int a, succeeded = 0, total = argc - 1;

  printf("adXtract 0.2\nextract ADX files from any archive, by recognizing signatures\n\n");

  if (total <= 0) {
	printf("usage: %s archive.zzz [ ... archive.zzz ]\n", argv[0]);
	return 1;
  }

  setvbuf(stdout, 0, _IONBF, 0);

  for (a = 1; a < argc; ++ a)
	succeeded += adxtract(argv[a]);

  if (succeeded == total) 
	return 0;
  else if (!succeeded)
	return 1;
  else 
	return 2; /* some failed but not all */

}
