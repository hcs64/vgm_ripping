#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

// ast_multi - split out X stereo streams from a 2X channel AST

#define BUFFER_SIZE (0x2760)

void try(const char * const what, int result) {
	if (!result) return;
	fprintf(stderr,"error: %s failed\n",what);
	exit(1);
}

unsigned int read32(unsigned char * buf) {
	return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
}
unsigned int read16(unsigned char * buf) {
	return (buf[0]<<8)|(buf[1]);
}
void write32(unsigned int n, unsigned char * buf) {
	buf[0]=n>>24;
	buf[1]=n>>16;
	buf[2]=n>>8;
	buf[3]=n;
}
void write16(unsigned int n, unsigned char * buf) {
	buf[0]=n>>8;
	buf[1]=n;
}

unsigned char dumpbuf[BUFFER_SIZE];
char namebase[PATH_MAX+1]={0},filename[PATH_MAX+5+1]={0};

void usage(const char * const binname) {
	fprintf(stderr,"usage: %s blah_multi.ast\n",binname);
}

int main(int argc, char ** argv) {
	FILE * infile, * outfile;
	int file_length;
	int i;
	int sample_count;
	int sample_rate;
	int channels;
	int loop_start=-1;
	int loop_end=-1;
	unsigned char headbuf[0x50];

	if (argc!=2) {usage(argv[0]); return 1;}

	{
		char * t;
		/* generate namebase */
		strncpy(namebase,argv[1],PATH_MAX);
		t = strrchr(namebase,'.');
		if (t) *t='\0';
	}
	
	
	try("open input file", (infile = fopen(argv[1],"rb"))==NULL);
	try("seek to file end", fseek(infile,0,SEEK_END));
	try("get file length", (file_length=ftell(infile))==-1);
	try("seek to file start", fseek(infile,0,SEEK_SET));
	try("read header", fread(headbuf,1,0x50,infile)!=0x50);	/* the header (0x40) and header of the first block (0x10) */

	try("format check",
		memcmp(headbuf, "STRM",4) ||		/* header */
		file_length-0x40 != read32(headbuf+4) ||	/* nothing in the file but header+data */
		memcmp(headbuf+0x40,"BLCK",4) ||		/* a valid first block */
		memcmp(dumpbuf+8,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",24));
		//memcmp(headbuf+0x48,"\0\0\0\0\0\0\0\0",8));	/* ADPCM files have stuff here */

	sample_rate=read32(headbuf+0x10);
	channels=read16(headbuf+0x0c);

	printf("%d Hz %d channels\n",sample_rate,channels);
	sample_count=read32(headbuf+0x14);
	printf("%d samples\n",sample_count);
	if (read16(headbuf+0x0e)) {
		loop_start=read32(headbuf+0x18);
		loop_end=read32(headbuf+0x1c);
		printf("loop start: %d samples\n",loop_start);
		printf("loop end: %d samples\n",loop_end);
	} else {
		printf("no loop\n");
	}

	if (channels==2) {
		printf("file is only 2 channels anyway, why would you want to split it?\n");
		return 1;
	}

	/* for each pair of 2 channels output an AST */
	for (i=0;i<channels;i+=2) {
		int current_sample;
		int outfile_length=0;
		FILE * outfile;

		try("seek to input stream start",fseek(infile,0x40,SEEK_SET));

		snprintf(filename,sizeof(filename)-1,"%s%d.ast",namebase,i/2);
		try("open output file",(outfile=fopen(filename,"wb"))==NULL);
		printf("writing %s\n",filename);
		
		try("write header",fwrite(headbuf,1,0x40,outfile)!=0x40);
		outfile_length+=0x40;

		for (current_sample=0; current_sample < sample_count; ) {
			int block_size;
			int chan;
			/*printf("%x\n",ftell(infile));*/
			try("read block header",fread(dumpbuf,1,0x20,infile)!=0x20);
			try("check block header",
				memcmp(dumpbuf,"BLCK",4) ||
				memcmp(dumpbuf+8,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",24));
			try("write block header",fwrite(dumpbuf,1,0x20,outfile)!=0x20);
			outfile_length+=0x20;

			block_size = read32(dumpbuf+4); /* bytes per channel */

			assert(block_size <= BUFFER_SIZE);
			for (chan=0;chan<channels;chan++) {
				/*printf("chan%d\n",chan);*/
				if ((chan/2)==(i/2)) {
					/*try("read samples",fread(dumpbuf,1,block_size,infile)!=block_size);*/
					int bytes_read = fread(dumpbuf,1,block_size,infile);
					if (bytes_read!=block_size) {
						if (current_sample+block_size/2>=sample_count && chan+1==channels) {
							int bytes_short = block_size-bytes_read;
							/* it seems that the last block is cut off? */
							printf("last block missing %#x bytes, filling with zero\n",bytes_short);
							memset(dumpbuf+bytes_read,0,bytes_short);
						} else {
							try("read samples",1);
						}
					}
					try("write samples",fwrite(dumpbuf,1,block_size,outfile)!=block_size);
					outfile_length+=block_size;
				} else {
					try("skip samples",fseek(infile,block_size,SEEK_CUR));
				}
			}
			current_sample+=block_size/2;
		}

		try("seek back to header (input)",fseek(infile,0,SEEK_SET));
		try("reread header",fread(dumpbuf,1,0x40,infile)!=0x40);
		try("seek back to header (output)",fseek(outfile,0,SEEK_SET));

		/* modify header */
		write16(2,dumpbuf+0x0c);	/* only two channels in output */
		write32(outfile_length-0x40,dumpbuf+0x04);	/* vastly different file size */

		try("write modified header",fwrite(dumpbuf,1,0x40,outfile)!=0x40);

		try("close output",fclose(outfile));
	}

	try("close input file", fclose(infile));
}
