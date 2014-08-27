#include <stdio.h>

/* get 16-bit big endian value */
unsigned int get16bit(unsigned char* p)
{
        return (p[0] << 8) | p[1];
}

/* get 32-bit big endian value */
unsigned int get32bit(unsigned char* p)
{
        return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

}

int readstring(FILE * infile, unsigned char * buf) {
    int i;
    for (i=0;fread(buf+i,1,1,infile)==1 && buf[i]; i++);
    return i; /* number of bytes read */
}

int main(int argc, char ** argv) {
    unsigned char buf[16];
    unsigned char catname[0x40],namebuf[0x60],extbuf[0x60];
    char fullname[0x40+3+0x60+1+0x60];
    FILE * infile;
    FILE * outfile;
    unsigned long filecount,offset,size,firstoffset;

    printf("jbc extractor 0.0\n\n");

    if (argc != 2 || !(infile=fopen(argv[1],"rb"))) {
	fprintf(stderr,"One way or another, you suck.\n");
	return 1;
    }

    fread(buf,1,16,infile);

    printf("?=0x%08x\n%d channels\n%d Hz\n?=0x%08x\n",get32bit(buf),get32bit(buf+4),get32bit(buf+8),get32bit(buf+12));

    fread(buf,1,16,infile);

    firstoffset=get32bit(buf);

    while (fread(buf,1,4,infile)==4 && ((filecount=get32bit(buf))!=0)) {
	unsigned long startoff=ftell(infile)-4;
	readstring(infile,catname);

	printf("category %s, %d files\n",catname,filecount);
	fseek(infile,startoff+0x40,SEEK_SET);

	for (;filecount>0;filecount--) {
	    startoff=ftell(infile);
	    fread(buf,1,12,infile);
	    offset=get32bit(buf);
	    size=get32bit(buf+4);
	    readstring(infile,namebuf);
	    readstring(infile,extbuf);
	    printf("\t%s.%s\toffset: 0x%08x\tsize: 0x%x\n",namebuf,extbuf,offset,size);

	    /* dump */
	    sprintf(fullname,"%s - %s.%s",catname,namebuf,extbuf);
	    outfile=fopen(fullname,"wb");
	    if (!outfile) {fprintf(stderr,"error opening %s for writing\n",fullname); return 1;}
	    
	    fseek(infile,offset,SEEK_SET);
	    while (size>=16 && fread(buf,1,16,infile) == 16) {
		fwrite(buf,1,16,outfile);
		size-=16;
	    }
	    if (size) {
		fread(buf,1,size,infile);
		fwrite(buf,1,size,outfile);
	    }
	    fclose(outfile);
	    
	    fseek(infile,startoff+0x60,SEEK_SET);
	}
    }

    fclose(infile);

    return 0;
}
