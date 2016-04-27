#include <stdlib.h>
#include <stdio.h>

/*
  fridge 0.0
  Extract files from FRID archives (.DPK)
  by hcs
*/

/*
  32-bit, little endian
*/
int read32bit(char * buffer) {
    unsigned char * buf = (unsigned char *)buffer;
    return buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);
}

int main(int argc, char ** argv) {
    char buf[16], * entry=NULL;
    FILE * infile=NULL, * outfile=NULL;
    int unk1; /* 0x04 */
    int datastart; /* 0x08 */
    int filecount; /* 0x0c */
    int datastart2; /* 0x10 */
    int entrylen; /* 0x14 */
    int i;

    printf("fridge 0.0\n\n");

    if (argc!=2) {fprintf(stderr,"usage: %s BLAH.DPK\n\n",argv[0]); return 1;}
    if (!(infile=fopen(argv[1],"rb"))) {fprintf(stderr,"error opening %s for input",argv[1]); return 1;}

    fread(buf,1,16,infile);
    unk1=read32bit(buf+4);
    if (memcmp(buf,"FRID",4)) {
	fprintf(stderr,"%s is not a FRID DPK file\n",argv[1]);
	return 1;
    }

    if (unk1 != 0xe0000000) fprintf(stderr,"expected 0xe0000000 at 0x04, got %#010x, moving on anyway...\n",unk1);

    datastart=read32bit(buf+8);
    printf("data starts at: %#010x\n",datastart);

    filecount=read32bit(buf+0xc);
    printf("%d files\n",filecount);

    fread(buf,1,16,infile);

    datastart2=read32bit(buf);
    if (datastart2!=datastart) fprintf(stderr,"expected %#010x at 0x10, got %#010x, moving on anyway...\n",datastart2);

    entrylen=read32bit(buf+4);
    printf("file entry length: %#x\n",entrylen);
    if (entrylen != 0x18) {
	fprintf(stderr,"I don't know how to deal with an entry length != 0x18\n");
	return 1;
    }

    if (read32bit(buf+8) || read32bit(buf+0xc)) fprintf(stderr,"expected 0 from 0x14-0x1f, got %#010x,%#010x, moving on anyway...\n",read32bit(buf+8),read32bit(buf+0xc));

    entry=(char*)malloc(entrylen);
    if (!entry) {fprintf(stderr,"error allocating memory for entry buffer, %#x bytes\n",entrylen); return 1;}

    for (i=0;i<filecount;i++) {
	int offset, size, unk2,memloc;
	fread(entry,1,entrylen,infile);
	offset=read32bit(entry+0xc);
	size=read32bit(entry+0x10);
	unk2=read32bit(entry+0x14);
	printf("file %d\n\t%s\n\toffset: %#010x\tsize: %#010x\tunk2: %#010x\n",i,entry,offset,size,unk2);

	outfile=fopen(entry,"wb");
	if (!outfile) {
	    fprintf(stderr,"error opening %s for output\n",entry);
	    return 1;
	}
	memloc=ftell(infile);

	fseek(infile,offset,SEEK_SET);

	while (size>=16) {
	    fread(buf,1,16,infile);
	    fwrite(buf,1,16,outfile);
	    size-=16;
	}
	if (size) {
	    fread(buf,1,size,infile);
	    fwrite(buf,1,size,outfile);
	}

	fclose(outfile);
	outfile=NULL;

	fseek(infile,memloc,SEEK_SET);
    }

    free(entry);
    entry=NULL;

    fclose(infile);
}
