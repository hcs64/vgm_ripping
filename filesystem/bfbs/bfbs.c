#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
  bfbs 0.0
  read filesystem in Beyond Good & Evil .bf files
  by hcs
*/

#ifdef WIN32
#define VERSION " win32 build"
#define DIRECTORY_SEP '\\'
#include <limits.h>
#else
#define VERSION ""
#define DIRECTORY_SEP '/'
#include <linux/limits.h>
#endif

/* read 32-bit integer, little endian */
int readint(char * buf) {
    return (((unsigned char)buf[0]&0xff)) | 
    (((unsigned char)buf[1]&0xff)<<8) |
    (((unsigned char)buf[2]&0xff)<<16) |
    (((unsigned char)buf[3]&0xff)<<24);
}

struct OFFSET {
    unsigned long offset;
    unsigned long unknown;
};

struct FILENAME {
    int unknown;
    int next;
    int prev;
    int directory;
    int unknown2;
    char name[0x40];
};

struct DIRNAME {
    int firstfile;
    int subdirs;
    int next;
    int prev;
    int parent;
    char name[0x40];
};

/* returns last character written (null) */
int stackofdirs(char * name, int maxlen, struct DIRNAME * dirs, int dir) {
    char * t;
    int n;
    if (dir!=-1) {
	n=stackofdirs(name,maxlen,dirs,dirs[dir].parent);
	for (t=dirs[dir].name;n<maxlen-2 && *t;t++,n++) name[n]=*t;
	name[n]=DIRECTORY_SEP;
	name[n+1]='\0';
	return n+1;
    }
    name[0]='\0';
    return 0;
}

/* return 1 if file already exists, 0 otherwise */
int openOutFile(char * inname, FILE ** outfile) {
    char * name;
    char * t;
    FILE * infile;
    name = malloc(strlen(inname)+1);
    strcpy(name,inname);
    for (t=name;*t;t++) {
	if (*t==DIRECTORY_SEP) {
	    *t='\0';
	    mkdir(name,0744);
	    *t=DIRECTORY_SEP;
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

int main(int argc, char ** argv) {
    FILE * infile=NULL, * outfile=NULL;
    char buf[8];
    struct OFFSET * offsets = NULL;
    struct FILENAME * files = NULL;
    struct DIRNAME * dirs = NULL;
    char name[PATH_MAX];

    int i,j;
    int fcount,dcount,tabsize;
    unsigned long taboffset,filenametab,dirnametab;
    unsigned long dataoffset;

    printf("bfbs 0.0 by hcs" VERSION "\n\n");

    if (argc!=2) {fprintf(stderr,"usage: %s sally.bf\n\n",argv[0]); return 1;}

    infile = fopen(argv[1],"rb");
    if (!infile) {fprintf(stderr,"error opening %s\n\n",argv[1]); return 1;}

    /* 0x00-0x03 */
    fread(buf,1,4,infile);
    if (memcmp(buf,"BIG",4)) {fprintf(stderr,"format error1\n\n"); return 1;}
    /* 0x04-0x07 */
    fread(buf,1,4,infile);
    if (readint(buf)!=0x22) {fprintf(stderr,"format error2\n\n"); return 1;}

    /* 0x08-0x0b */
    fread(buf,1,4,infile);
    fcount=readint(buf);
    /* 0x0c-0x0f */
    fread(buf,1,4,infile);
    dcount=readint(buf);

    /* 0x10-0x17 */
    fread(buf,1,8,infile);
    if (readint(buf)!=0) {fprintf(stderr,"format error3\n\n"); return 1;}
    /* 0x18-0x1f */
    fread(buf,1,8,infile);
    if (readint(buf)!=-1) {fprintf(stderr,"format error4\n\n"); return 1;}

    /* 0x20-0x23 */
    fread(buf,1,4,infile);
    tabsize=readint(buf);

    /* 0x24-0x27 */
    fread(buf,1,4,infile);
    if (readint(buf)!=1) {fprintf(stderr,"format error5\n\n"); return 1;}
    /* 0x28-0x2b */
    fread(buf,1,4,infile);
    if (readint(buf)!=0x71003ff9) {fprintf(stderr,"format error6\n\n"); return 1;}
    /* 0x2c-0x2f */
    fread(buf,1,4,infile);
    if (readint(buf)!=fcount) {fprintf(stderr,"format error7\n\n"); return 1;}
    /* 0x30-0x33 */
    fread(buf,1,4,infile);
    if (readint(buf)!=dcount) {fprintf(stderr,"format error8\n\n"); return 1;}

    /* 0x34-0x37 */
    fread(buf,1,4,infile);
    taboffset=readint(buf);
    if (taboffset!=0x44) {fprintf(stderr,"format error9\n\n"); return 1;}

    /* 0x38-0x3b */
    fread(buf,1,4,infile);
    if (readint(buf)!=-1) {fprintf(stderr,"format error10\n\n"); return 1;}
    /* 0x3c-0x3f */
    fread(buf,1,4,infile);
    if (readint(buf)!=0) {fprintf(stderr,"format error11\n\n"); return 1;}

    /* 0x40-0x43 */
    fread(buf,1,4,infile);
    if (readint(buf)!=tabsize-1) {fprintf(stderr,"format error12\n\n"); return 1;}

    printf("file: %s\n",argv[1]);
    printf("file count: %#8x\n",fcount);
    printf("dir count: %#8x\n",dcount);

    filenametab=taboffset+tabsize*8;
    dirnametab=filenametab+tabsize*0x54;

    /* allocate */
    offsets = malloc(sizeof(struct OFFSET)*fcount);
    files = malloc(sizeof(struct FILENAME)*fcount);
    dirs = malloc(sizeof(struct DIRNAME)*dcount);
    if (!offsets || !files || ! dirs) {fprintf(stderr,"unable to allocate memory for filesystem\n\n"); return 1;}

    /* dump offsets table */
    fseek(infile,taboffset,SEEK_SET);
    for (i=0;i<fcount;i++) {
	fread(buf,1,4,infile);
	offsets[i].offset=readint(buf);
	fread(buf,1,4,infile);
	offsets[i].unknown=readint(buf);
    }

    /* retrieve file names & data */
    fseek(infile,filenametab,SEEK_SET);
    for (i=0;i<fcount;i++) {
	fread(buf,1,4,infile);
	files[i].unknown=readint(buf);
	fread(buf,1,4,infile);
	files[i].next=readint(buf);
	fread(buf,1,4,infile);
	files[i].prev=readint(buf);
	fread(buf,1,4,infile);
	files[i].directory=readint(buf);
	fread(buf,1,4,infile);
	files[i].unknown2=readint(buf);

	fread(files[i].name,1,0x40,infile);
    }

    /* retrieve directory names & data */
    fseek(infile,dirnametab,SEEK_SET);
    for (i=0;i<dcount;i++) {
	fread(buf,1,4,infile);
	dirs[i].firstfile=readint(buf);
	fread(buf,1,4,infile);
	dirs[i].subdirs=readint(buf);
	fread(buf,1,4,infile);
	dirs[i].next=readint(buf);
	fread(buf,1,4,infile);
	dirs[i].prev=readint(buf);
	fread(buf,1,4,infile);
	dirs[i].parent=readint(buf);

	fread(dirs[i].name,1,0x40,infile);
    }

    /* list files */
    for (i=0;i<fcount;i++) {
	int offset,size;
	fseek(infile,offsets[i].offset,SEEK_SET);
	fread(buf,1,4,infile);
	offset=offsets[i].offset+4;
	size=readint(buf);
	stackofdirs(name,sizeof(name)/sizeof(char),dirs,files[i].directory);
	strcat(name,files[i].name);
	printf("%s\n",name,sizeof(name)/sizeof(char)-1);

	printf("offset: %#8x\tsize: %#8x\toffset.unk: %#8x\tfiles.unk: %#8x\tfiles.unk2: %#8x\n",offset,size,offsets[i].unknown,files[i].unknown,files[i].unknown2);

	if (openOutFile(name,&outfile)) {
	    /* noclobber */
	} else {
	    while (size>=8) {
		fread(buf,1,8,infile);
		fwrite(buf,1,8,outfile);
		size-=8;
	    }
	    fread(buf,1,size,infile);
	    fwrite(buf,1,size,outfile);
	    fclose(outfile);
	}
	outfile=NULL;

    }

    free(offsets); offsets=NULL;
    free(files); files=NULL;
    free(dirs); dirs=NULL;
    
    return 0;
}
