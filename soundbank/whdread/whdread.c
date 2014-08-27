#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* WHD (Wave HeaDer) reader 0.2 by hcs */

/* the separator used in the host OS */

#define DIR_SEPARATOR_CHAR	'\\'

unsigned short get16bit(unsigned char * buf) {
    return (buf[0]<<8)|(buf[1]);
}

unsigned long get32bit(unsigned char * buf) {
    return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
}

/*
  maxlen is the size of *namebuf
  returns the offset of the start of the name in the file
*/
long getNameBackwards(FILE * infile, unsigned char * namebuf, int maxlen) {
    int i,j;
    char buf;
    if (!maxlen) return ftell(infile);
    while (fread(&buf,1,1,infile)==1 && buf==0) {fseek(infile,-2,SEEK_CUR);}

    namebuf[0]='\0';
    for (i=1;;i++) {
	if (i<maxlen) namebuf[i]=buf;
	fseek(infile,-2,SEEK_CUR);
	fread(&buf,1,1,infile);
	if (buf==1 || buf==0) break;
    }
    i++;
    if (i>=maxlen) i=maxlen-1;

    for (j=0;j<i/2;j++) {
	char temp=namebuf[j];
	namebuf[j]=namebuf[i-1-j];
	namebuf[i-1-j]=temp;
    }
    return ftell(infile);

}

/*
  create directories leading up to file, handle path seperators
  (in WHD they are always \ )

  return 1 if file already exists, 0 otherwise
*/
int openOutFile(char * inname, FILE ** outfile) {
    char * name;
    char * t;
    FILE * infile;
    name = malloc(strlen(inname)+1);
    strcpy(name,inname);
    for (t=name;*t;t++) {
	if (*t=='\\') {
	    *t='\0';
	    mkdir(name,0744);
	    *t=DIR_SEPARATOR_CHAR;
	}
    }
    if (!(infile=fopen(name,"rb"))) {
    	*outfile = fopen(name,"wb");
	free(name);
	return 0;
    } else {
	fclose(infile);
	return 1;
    }
}

int main(int argc, char * argv[]) {
    FILE * inwhd, * inwav = NULL, * instream, * outfile;
    int offset,type,fileoffset,filesize;
    unsigned char buf[16],namebuf[256];
    int i;

    printf("whdread 0.2\n\n");

    if (argc != 3 && argc != 4) {
	fprintf(stderr,"Usage: whdread foo.WHD streams.wav [foo.WAV]\n");
	return 1;
    }
    inwhd = fopen(argv[1],"rb");
    if (!inwhd) {
	fprintf(stderr,"Error opening %s for reading\n",argv[1]);
	return 1;
    }
    instream = fopen(argv[2],"rb");
    if (!instream) {
	fprintf(stderr,"Error opening %s for reading\n",argv[2]);
	return 1;
    }
    if (argc == 4) {
	inwav = fopen(argv[3],"rb");
	if (!inwav) {
	    fprintf(stderr,"Error opening %s for reading\n",argv[3]);
	    return 1;
	}
    } else {
	char * t;
	t=strrchr(argv[1],'.');
	if (!t) {fprintf(stderr,"couldn't guess a corresponding WAV for %s\n",argv[1]); return 1;}
	t[1]='W';
	t[2]='A';
	t[3]='V';
	printf("Guessing %s as wav\n",argv[1]);
	inwav = fopen(argv[1],"rb");
	if (!inwav) {
	    fprintf(stderr,"Error opening %s for reading\n",argv[1]);
	    /*return 1;*/
	}
    }

    fseek(inwhd,0,SEEK_SET);
    fread(buf,1,8,inwhd);

    printf("WHD Size: %d\nLast offset: 0x%x\n\n",get32bit(buf+4),get32bit(buf));

    offset=get32bit(buf);
    fseek(inwhd,offset-4,SEEK_SET); /* seek to last offset */

    while (fread(buf,1,4,inwhd)==4) {
	type=get32bit(buf);
	if (type==0) { /* DSP */
	    int i;
	    int coef;
	    char headbuf[0x60];
	    fseek(inwhd,offset-8,SEEK_SET);
	    fread(buf,1,4,inwhd);
	    coef=get32bit(buf); /* offset of DSP coefficients */
	    offset=coef-1;
	    fseek(inwhd,coef+0x50,SEEK_SET);
	    fread(buf,1,8,inwhd);
	    fileoffset=get32bit(buf);
	    filesize=(get32bit(buf+4)/14+1)*8;
	    fseek(inwhd,offset,SEEK_SET);
	    offset=getNameBackwards(inwhd,namebuf,sizeof(namebuf));

	    fseek(inwhd,coef,SEEK_SET);

	    /* do nothing if file already exists */
	    if (inwav && !openOutFile((char*)namebuf,&outfile)) {
		if (!outfile) {
		    fprintf(stderr,"Error opening %s for output\n",namebuf);
		    return 1;
		}

		fread(headbuf,1,0x60,inwhd);
		fwrite(headbuf,1,0x60,outfile);

		fseek(inwav,fileoffset,SEEK_SET);

		for (i=filesize;i>=16;i-=16) {
		    fread(buf,1,16,inwav);
		    fwrite(buf,1,16,outfile);
		}
		if (i) {
		    fread(buf,1,i,inwav);
		    fwrite(buf,1,i,outfile);
		}

		fclose(outfile);
	    }
	} else if (type==1) { /* 16-bit big endian PCM */
	    int oldoffset,i;
	    char headbuf[0x30];
	    offset-=0x32;
	    fseek(inwhd,offset+0x22,SEEK_SET);
	    fread(buf,1,8,inwhd);
	    fileoffset=get32bit(buf);
	    filesize=get32bit(buf+4)*2; /* counted in samples */
	    fseek(inwhd,offset,SEEK_SET);
	    oldoffset=offset;
	    offset=getNameBackwards(inwhd,namebuf,sizeof(namebuf));

	    fseek(inwhd,oldoffset+2,SEEK_SET);

	    /* do nothing if file already exists */
	    if (!openOutFile((char*)namebuf,&outfile)) {
		if (!outfile) {
		    fprintf(stderr,"Error opening %s for output\n",namebuf);
		    return 1;
		}

		fread(headbuf,1,0x30,inwhd);
		fwrite(headbuf,1,0x30,outfile);

		fseek(instream,fileoffset,SEEK_SET);

		for (i=filesize;i>=16;i-=16) {
		    fread(buf,1,16,instream);
		    fwrite(buf,1,16,outfile);
		}
		if (i) {
		    fread(buf,1,i,instream);
		    fwrite(buf,1,i,outfile);
		}

		fclose(outfile);
	    }
	    
	} else break;

	if (type==0) {
	    printf("****\ntype=DSP\nname=%s\n",namebuf);
	    printf("offset=0x%x\nsize=0x%x\n",fileoffset,filesize);
	}
	if (type==1) {
	    printf("****\ntype=PCM\nname=%s\n",namebuf);
	    printf("offset=0x%x\nsize=0x%x\n",fileoffset,filesize);
	}
	printf("\n");

	fseek(inwhd,offset-4,SEEK_SET);
    }

    return 0;
}
