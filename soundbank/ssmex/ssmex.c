// SSM extractor v0.1 by hcs
// tested on Super Smash Bros. Melee, one track from bigblue.ssm is missing because of 0% confidence
// 09/19/05

#include <stdio.h>
#include <string.h>

// head
typedef struct {
	short loop_flag;	// 0
	short format;		// 2
	long sa;			// 4
	long ea;			// 8
	long ca;			// 0xc
	short coef[0x10];	// 0x10
	short gain;			// 0x30
	short initps;		// 0x32
	short initsh1;		// 0x34
	short initsh2;		// 0x36
	short loopps;		// 0x38
	short loopsh1;		// 0x3a
	short loopsh2;		// 0x3c
	short pad;			// 0x3e
} DSPHEAD;

char readbuf[1024];

unsigned short get16bit(unsigned char * b) {
	return (b[0]<<8) | b[1];
}

unsigned long get32bit(unsigned char * b) {
	return (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];
}

unsigned long revend32(unsigned long i) {
	return ((i>>24)&0xff) | ((i>>8)&0xff00) | ((i<<8)&0xff0000) | ((i<<24)&0xff000000);
}

unsigned short revend16(unsigned short i) {
	return ((i>>8)&0xff) | ((i<<8)&0xff00);
}

void writeDSPhead(DSPHEAD dsp, long srate, FILE * outfile) {
	unsigned long lt;
		
	// # of samples
	lt=revend32((revend32(dsp.ea)-revend32(dsp.sa))*7/8);
	fwrite(&lt,4,1,outfile);

	// # of nibbles
	lt=revend32(revend32(dsp.ea)-revend32(dsp.sa));
	fwrite(&lt,4,1,outfile);

	// srate
	lt=revend32(srate);
	fwrite(&lt,4,1,outfile);

	dsp.ea=revend32(revend32(dsp.ea)-revend32(dsp.sa)-2);
	dsp.sa=revend32(2);
	dsp.ca=revend32(2);

	fwrite(&dsp,0x40,1,outfile);
}

void writeDSPfile(DSPHEAD dsp, long srate, long dataend, FILE * infile, FILE * outfile) {
	writeDSPhead(dsp,srate,outfile);

	fseek(outfile,0x60,SEEK_SET);
	while (ftell(infile)<dataend) {
		fread(readbuf,1,4,infile);
		fwrite(readbuf,1,4,outfile);
	}

}

int main(int argc, char ** argv) {
	FILE * infile, * outfile;
	DSPHEAD dsp,dsp2;
	char namebase[1024], *t;
	char fname[256];
	int headoff,headsize,dataoff,dataoff2,realdataoff,dataend,dspcount;
	int NCH,freq;
	int c,i,j;
	int conf;

	printf("SSM extractor v0.1 by hcs\n\n");

	if (argc!=2) {printf("usage: %s in.ssm\n",argv[0]); return 1;}

	infile = fopen(argv[1],"rb");
	if (!infile) {printf("error opening %s\n",argv[1]); return 1;}

	printf("Reading %s\n",argv[1]);

	// generate namebase
	t=strrchr(argv[1],'\\');
	if (!t) t=argv[1];
	else t++;
	for (i=0;t<strrchr(argv[1],'.');t++,i++) namebase[i]=*t;
	namebase[i]='\0';
	
	// read header
	fread(readbuf,1,1024,infile);
	
	fseek(infile,0,SEEK_END);
	realdataoff=ftell(infile)-get32bit(readbuf+4);

	dataoff=get32bit(readbuf);
	
	dspcount=get32bit(readbuf+8);
	printf("data offset: %x\nreal data offset: %x\nDSP count: %d\n",dataoff,realdataoff,dspcount);
	
	//

	dataoff=realdataoff;
	headsize=realdataoff;
	headoff=0x10;

	for (c=0;c<dspcount;c++) {
		// mono header size 0x48
		// stereo header size 0x88
		// first 8 is the header for the header...

		fseek(infile,headoff,SEEK_SET);
		fread(readbuf,1,8,infile);
		NCH=get32bit(readbuf);
		freq=get32bit(readbuf+4);
		printf ("DSP #%02x: %dHz ",c,freq);
		
		if (NCH==1) printf("mono");
		else printf("stereo");

		printf(" ");

		headoff+=8;

		fread(&dsp,1,0x40,infile);
		if (NCH==2) {
			fseek(infile,headoff+0x40,SEEK_SET);
			fread(&dsp2,1,0x40,infile);
		}
		//dataoff=((revend32(dsp.sa)/2)+(freq==8000?-0x260:0)+headsize+1)&(~0x7);
		dataoff2=((revend32(dsp.sa)/2)+headsize+1)&(~0x7);

		if (dataoff==dataoff2) conf=100;
		else conf=67;

		fseek(infile,dataoff,SEEK_SET);
		fread(readbuf,1,4,infile);
		if (readbuf[0]!=revend16(dsp.initps)) {/*printf("******** Mismatch 1\n"); */ conf/=2;}
		else realdataoff=dataoff;
		
		fseek(infile,dataoff2,SEEK_SET);
		fread(readbuf,1,4,infile);
		if (readbuf[0]!=revend16(dsp.initps)) {/*printf("******** Mismatch 2\n"); */ conf/=2;}
		else realdataoff=dataoff2;

		if (conf < 20) conf=0;

		if (conf==0) realdataoff=dataoff;

		dataend=(((revend32(dsp.ea)/2)+headsize)&(~0x7))+8;
		if (NCH==2) dataend=(((revend32(dsp2.ea)/2)+headsize)&(~0x7))+8;
		
		//printf ("initps:%04x dataoff:%08x dataoff2:%08x\n",revend16(dsp.initps),dataoff,dataoff2);
		printf(" start: %08x end %08x confidence=%d%%\n",realdataoff,dataend,conf);

		if (conf) {
			if (NCH==2) {
				sprintf(fname,"%s%02xL.dsp",namebase,c);

				outfile=fopen(fname,"wb");
				fseek(infile,realdataoff,SEEK_SET);
				writeDSPfile(dsp,freq,(realdataoff+dataend)/2,infile,outfile);

				fclose(outfile);

				sprintf(fname,"%s%02xR.dsp",namebase,c);

				outfile=fopen(fname,"wb");
				fseek(infile,(realdataoff+dataend)/2,SEEK_SET);
				writeDSPfile(dsp,freq,dataend,infile,outfile);

				fclose(outfile);
			} else {
				sprintf(fname,"%s%02x.dsp",namebase,c);
				outfile=fopen(fname,"wb");
				fseek(infile,realdataoff,SEEK_SET);
				writeDSPfile(dsp,freq,dataend,infile,outfile);

				fclose(outfile);
			}
		} /*else {
			sprintf(fname,"BAD%s%02x.dsp",namebase,c);
			outfile=fopen(fname,"wb");
			fclose(outfile);
		}*/

		headoff+=NCH*0x40;

		dataoff=dataend;
	}

	printf("\n\n");

	fclose(infile);
	return 0;
}