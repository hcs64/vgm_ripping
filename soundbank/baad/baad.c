#include <stdio.h>
#include <limits.h>
#include <string.h>

/* Parse and dump from BAA (Twilight Princess Audio Archive).
 * This file contains a subfiles which contain audio metadata.
 *
 * I only have one .baa for reference so this may well be wrong, but
 * it seems to be very simple. Simple enough that there isn't a direct
 * listing of the size of certain files, so I use the known order of
 * Z2Sound.baa to determine the limits.
 * In a lot of cases I also assume there is only one of each chunk type
 * when naming the output files.
 */

#define DUMPBUFSIZE 512

int read32(unsigned char * buf) {
	return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
}

int dump(FILE * infile, FILE * outfile, int offset, int size) {
	char dumpbuf[DUMPBUFSIZE];
	int oldoff;
	oldoff=ftell(infile);

	fseek(infile,offset,SEEK_SET);

	for (;size>=DUMPBUFSIZE;size-=DUMPBUFSIZE) {
		fread(dumpbuf,1,DUMPBUFSIZE,infile);
		fwrite(dumpbuf,1,DUMPBUFSIZE,outfile);
	}
	if (size) {
		fread(dumpbuf,1,size,infile);
		fwrite(dumpbuf,1,size,outfile);
	}
	fseek(infile,oldoff,SEEK_SET);
}

int create_and_dump(FILE * infile, const char * filename, int offset, int size) {
	FILE * outfile = fopen(filename,"wb");
	if (!outfile) {
		fprintf(stderr,"error opening %s for writing\n",filename);
		return 1;
	}

	if (dump(infile,outfile,offset,size)) {
		fprintf(stderr,"error dumping to %s\n",filename);
		fclose(outfile);
		return 1;
	}
	fclose(outfile);
	return 0;
}

int main(int argc, char ** argv) {
	FILE * infile;
	unsigned char buf[4];
	int thesunshines=1;

	int ibnkcount=0;

	/* read .baa and dump .bst and .bstn */

	if (argc!=2) {
		fprintf(stderr,"arg!\n");
		return 1;
	}
	infile=fopen(argv[1],"rb");
	if (!infile) {
		fprintf(stderr,"fail open %s\n",argv[1]);
		return 1;
	}

	fread(buf,1,4,infile);
	if (memcmp(buf,"AA_<",4)) {
		fprintf(stderr,"format not recognized\n");
		return 1;
	}

	while (thesunshines) {
		char filename[PATH_MAX+1];
		memset(filename,0,PATH_MAX+1);

		/* read chunk name */
		fread(buf,1,4,infile);
		if (!memcmp(buf,"bst ",4)) {
			int bstoffset;
			int bstnoffset;

			printf("\"bst \"\t");
			fread(buf,1,4,infile);
			bstoffset=read32(buf);
			printf("%08x\t",read32(buf));

			fread(buf,1,4,infile);
			bstnoffset=read32(buf);
			printf("%08x\n",read32(buf));

			snprintf(filename,PATH_MAX,"%s.bst",argv[1]);
			if (create_and_dump(infile,filename,bstoffset,bstnoffset-bstoffset)) return 1;
		} else if (!memcmp(buf,"bstn",4)) {
			int bstnoffset;
			int bstnendoffset;

			printf("\"bstn\"\t");
			fread(buf,1,4,infile);
			bstnoffset=read32(buf);
			printf("%08x\t",read32(buf));

			fread(buf,1,4,infile);
			bstnendoffset=read32(buf);
			printf("%08x\n",read32(buf));

			snprintf(filename,PATH_MAX,"%s.bstn",argv[1]);
			if (create_and_dump(infile,filename,bstnoffset,bstnendoffset-bstnoffset)) return 1;
		} else if (!memcmp(buf,"ws  ",4)) {
			int wsoffset;
			int wssize;
			int wstype;
			int oldoff;

			printf("\"ws  \"\t");
			fread(buf,1,4,infile);
			wstype=read32(buf);
			printf("%08x\t",read32(buf));
			fread(buf,1,4,infile);
			wsoffset=read32(buf);
			printf("%08x\t",read32(buf));
			fread(buf,1,4,infile);
			printf("%08x\n",read32(buf));
			
			oldoff=ftell(infile);

			fseek(infile,wsoffset+4,SEEK_SET);
			fread(buf,1,4,infile);
			wssize=read32(buf);

			fseek(infile,oldoff,SEEK_SET);

			snprintf(filename,PATH_MAX,"%s.%d.wsys",argv[1],wstype);

			if (create_and_dump(infile,filename,wsoffset,wssize)) return 1;
		} else if (!memcmp(buf,"bnk ",4)) {
			int bnkoffset;
			int bnktype;
			int bnklen;
			int oldoff;
			oldoff=ftell(infile)+8;

			printf("\"bnk \"\t");
			fread(buf,1,4,infile);
			bnktype=read32(buf);
			printf("%08x\t",read32(buf));

			fread(buf,1,4,infile);
			bnkoffset=read32(buf);
			printf("%08x\n",read32(buf));

			fseek(infile,bnkoffset+4,SEEK_SET);
			fread(buf,1,4,infile);
			bnklen=read32(buf);

			fseek(infile,oldoff,SEEK_SET);


			snprintf(filename,PATH_MAX,"%s.%d_%d.bnk",argv[1],bnktype,ibnkcount++);
			if (create_and_dump(infile,filename,bnkoffset,bnklen)) return 1;
		} else if (!memcmp(buf,"bsc ",4)) {
			int bscoffset;
			int bscend;

			printf("\"bsc \"\t");
			fread(buf,1,4,infile);
			bscoffset=read32(buf);
			printf("%08x\t",read32(buf));

			fread(buf,1,4,infile);
			bscend=read32(buf);
			printf("%08x\n",read32(buf));

			snprintf(filename,PATH_MAX,"%s.bsc",argv[1]);
			if (create_and_dump(infile,filename,bscoffset,bscend-bscoffset)) return 1;
		} else if (!memcmp(buf,"bfca",4)) {
			/* something involving streams? */
			printf("\"bfca\"\t");
			fread(buf,1,4,infile);
			printf("%08x\n",read32(buf));
		} else if (!memcmp(buf,">_AA",4)) {
			/* haymaking time is over */
			thesunshines=0;
			printf("\">_AA\"\n");
		} else {
			fprintf(stderr,"unrecognized chunk, %c%c%c%c (%08x)\n",
					buf[0],buf[1],buf[2],buf[3],read32(buf));
			return 1;
		}
	}

	return 0;
}
