#include <stdio.h>
#include <string.h>

int read32(unsigned char * buf) {
	return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
}

#define FILENAMESIZE 256
#define BUFSIZE 256
int traverse(FILE * inbst, int bstoff, FILE * inbstn, int bstnoff, const char * dirname, int leafaddr) {
	char filename[FILENAMESIZE];
	char buf[BUFSIZE];
	int count;
	int i;
	int leaf=0;

	memset(filename,0,FILENAMESIZE);

	if (leafaddr) {
		int next;
		fseek(inbstn,bstnoff,SEEK_SET);
		fread(filename,1,FILENAMESIZE-1,inbstn);
		fseek(inbst,bstoff&0xffffff,SEEK_SET);
		fread(buf,1,4,inbst);
		printf("[%08x]=%08x=%08x\t%s%s\n",leafaddr,bstoff,read32(buf),dirname,filename);
		return 0;
	}


	fseek(inbst,bstoff,SEEK_SET);
	fseek(inbstn,bstnoff,SEEK_SET);

	fread(buf,1,4,inbst); bstoff+=4;
	count=read32(buf);
	fread(buf,1,4,inbstn); bstnoff+=4;

	if (read32(buf)!=count) {
		fprintf(stderr,"inconsistency with %s subdirectory count, %d!=%d\n",dirname,read32(buf),count);
		return 1;
	}

	/* Leaf directories in BST start with 0 */
	fread(buf,1,4,inbst); bstoff+=4;
	if (!read32(buf)) leaf=1;
	else bstoff-=4;

	/* get my name */
	if (dirname) {
		int nameoff;
		fread(buf,1,4,inbstn); bstnoff+=4;
		nameoff=read32(buf);
		fseek(inbstn,nameoff,SEEK_SET);
		fread(buf,1,BUFSIZE,inbstn);
		buf[BUFSIZE-1]='\0';
		snprintf(filename,FILENAMESIZE-1,"%s%s/",dirname,buf);
	} else {
		/* root has no name */
		snprintf(filename,FILENAMESIZE-1,"/");
	}

	for (i=0;i<count;i++) {
		int nextbstoff,nextbstnoff;
		fseek(inbst,bstoff+i*4,SEEK_SET);
		fseek(inbstn,bstnoff+i*4,SEEK_SET);

		fread(buf,1,4,inbst);
		nextbstoff=read32(buf);
		fread(buf,1,4,inbstn);
		nextbstnoff=read32(buf);

		if (leaf) {
			if (traverse(inbst,nextbstoff,inbstn,nextbstnoff,filename,bstoff+i*4))
				return 1;
		} else {
			if (traverse(inbst,nextbstoff,inbstn,nextbstnoff,filename,0))
				return 1;
		}
	}
	return 0;
}

int main(int argc, char ** argv) {
	FILE * inbst = NULL, * inbstn = NULL;
	char buf[4];
	int bstoff, bstnoff;

	if (argc!=3) {
		fprintf(stderr,"arg!\n");
		return 1;
	}

	inbst=fopen(argv[1],"rb");
	if (!inbst) {
		fprintf(stderr,"failed open %s\n",argv[1]);
		return 1;
	}
	inbstn=fopen(argv[2],"rb");
	if (!inbstn) {
		fprintf(stderr,"failed open %s\n",argv[2]);
		return 1;
	}

	fread(buf,1,4,inbst);
	if (memcmp(buf,"BST ",4)) {
		fprintf(stderr,"%s not recognized as BST\n",argv[1]);
		return 1;
	}
	fread(buf,1,4,inbstn);
	if (memcmp(buf,"BSTN",4)) {
		fprintf(stderr,"%s not recognized as BSTN\n",argv[2]);
		return 1;
	}

	fread(buf,1,4,inbst);
	if (read32(buf)) {
		fprintf(stderr,"%s not recognized as BST\n",argv[1]);
		return 1;
	}
	fread(buf,1,4,inbstn);
	if (read32(buf)) {
		fprintf(stderr,"%s not recognized as BSTN\n",argv[1]);
		return 1;
	}

	fread(buf,1,4,inbst);
	if (read32(buf)!=0x01000000) {
		fprintf(stderr,"%s not recognized as BST\n",argv[1]);
		return 1;
	}
	fread(buf,1,4,inbstn);
	if (read32(buf)!=0x01000000) {
		fprintf(stderr,"%s not recognized as BSTN\n",argv[1]);
		return 1;
	}

	fread(buf,1,4,inbst);
	bstoff=read32(buf);
	fread(buf,1,4,inbstn);
	bstnoff=read32(buf);

	traverse(inbst, bstoff, inbstn, bstnoff, NULL, 0);
}
