/*
 * wwdumpsnd 0.4 by hcs
 * dump audio from Wind Waker or Super Mario Sunshine
 * needs JaiInit.aaf and *.aw in current directory
 * (if Sunshine, the file is 'msound.aaf', from 'nintendo.szs',
 *  but you'll need to rename it :))
 */

#include <stdio.h>
#include <string.h>

typedef signed short s16;

/* read big endian */
unsigned int read32(unsigned char * buf) {
	return (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
}

/* write little endian (for WAV) */
void write32le(int in, unsigned char * buf) {
	buf[0]=in&0xff;
	buf[1]=(in>>8)&0xff;
	buf[2]=(in>>16)&0xff;
	buf[3]=(in>>24)&0xff;
}

/* AFC decoder */

const short afccoef[16][2] =
{{0,0},
{0x0800,0},
{0,0x0800},
{0x0400,0x0400},
{0x1000,0xf800},
{0x0e00,0xfa00},
{0x0c00,0xfc00},
{0x1200,0xf600},
{0x1068,0xf738},
{0x12c0,0xf704},
{0x1400,0xf400},
{0x0800,0xf800},
{0x0400,0xfc00},
{0xfc00,0x0400},
{0xfc00,0},
{0xf800,0}};


/* from Dolphin's "UCode_Zelda_ADPCM.cpp", r7504 */
void AFCdecodebuffer(const s16 *coef, const char *src, signed short *out, short *histp, short *hist2p, int type)
{
    int i;
        // First 2 nibbles are ADPCM scale etc.
    short delta = 1 << (((*src) >> 4) & 0xf);
    short idx = (*src) & 0xf;
    src++;

        short nibbles[16];
    if (type == 9)
    {
        for (i = 0; i < 16; i += 2)
                {
            nibbles[i + 0] = *src >> 4;
            nibbles[i + 1] = *src & 15;
            src++;
        }
        for (i = 0; i < 16; i++) {
            if (nibbles[i] >= 8) 
                nibbles[i] = nibbles[i] - 16;
                        nibbles[i] <<= 11;
        }
    }
    else
    {
                // In Pikmin, Dolphin's engine sound is using AFC type 5, even though such a sound is hard
                // to compare, it seems like to sound exactly like a real GC
                // In Super Mario Sunshine, you can get such a sound by talking to/jumping on anyone
        for (i = 0; i < 16; i += 4)
        {
            nibbles[i + 0] = (*src >> 6) & 0x03;
            nibbles[i + 1] = (*src >> 4) & 0x03;
            nibbles[i + 2] = (*src >> 2) & 0x03;
            nibbles[i + 3] = (*src >> 0) & 0x03;
            src++;
        }

        for (i = 0; i < 16; i++) 
        {
            if (nibbles[i] >= 2) 
                nibbles[i] = nibbles[i] - 4;
                        nibbles[i] <<= 13;
        }
    }

        short hist = *histp;
    short hist2 = *hist2p;
    for (i = 0; i < 16; i++)
        {
        int sample = delta * nibbles[i] + ((int)hist * coef[idx * 2]) + ((int)hist2 * coef[idx * 2 + 1]);
        sample >>= 11;
        if (sample > 32767)
            sample = 32767;
        if (sample < -32768)
            sample = -32768;
        out[i] = sample;
        hist2 = hist;
        hist = (short)sample;
    }
        *histp = hist;
    *hist2p = hist2;
}

/* dump a WAV, decoding AFC */
/* return 0 on success, 1 on failure */
int dumpAFC(FILE * const infile, const int offset, const int size, const int srate, const int type, const char * const filename) {
	long oldpos;
	char inbuf[9];
	short outbuf[16];
	FILE * outfile;
	int sizeleft;
	int framesize;
	int outsize,outsizetotal;
	short hist=0,hist2=0;
	
	unsigned char wavhead[44] = {
		0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,  0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
		0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61,  0x00, 0x00, 0x00, 0x00
	};

	oldpos = ftell(infile);
	if (oldpos < 0) return 1;

	outfile = fopen(filename,"wb");
	if (!outfile) return 1;

	framesize = (type==5) ? 5 : 9;
	outsize = size/framesize*16*2;
	outsizetotal = outsize+8;
	write32le(outsizetotal,wavhead+4);
	write32le(outsize,wavhead+40);
	write32le(srate,wavhead+24);
	write32le(srate*2,wavhead+28);
	if (fwrite(wavhead,1,44,outfile)!=44) return 1;

	if (fseek(infile,offset,SEEK_SET)<0) return 1;

	for (sizeleft=size;sizeleft>=framesize;sizeleft-=framesize) {
		if (fread(inbuf,1,framesize,infile) != framesize)
			return 1;

		AFCdecodebuffer((s16*)afccoef,inbuf,outbuf,&hist,&hist2,type);

		if (fwrite(outbuf,1,16*2,outfile) != 16*2)
			return 1;
	}

	if (fclose(outfile)==EOF) return 1;

	if (fseek(infile,oldpos,SEEK_SET)<0) return 1;

	return 0;
}

int verbose = 0;

int doaw(FILE *infile, const int offset) {
	FILE * awfile;
	int next_aw_offset;
	unsigned char buf[4];
	int aw_name;
	int table_offset;
	int wav_count;
	int i;
	char fname[113]={0};
	int type;

	/* offset to list of wave table entry offsets */
	if (fread(buf,1,4,infile)!=4) return 1;
	aw_name = read32(buf) + offset;
	table_offset = aw_name+112;

	next_aw_offset = ftell(infile);
	if (next_aw_offset<0) return 1;

	if (fseek(infile,aw_name,SEEK_SET)<0) return 1;

	/* aw file name */
	if (fread(fname,1,112,infile)!=112) return 1;
	awfile = fopen(fname,"rb");
	if (!awfile) return 1;

	/* number of waves */
	if (fread(buf,1,4,infile)!=4) return 1;
	wav_count = read32(buf);

	if (verbose) {
		printf("aw=%s\n",fname);
		printf("table at %x, wav_count=%x\n",table_offset,wav_count);
	}

	for (i=0;i<wav_count;i++) {
		int wav_entry_offset;
		int afcoffset,afcsize,srate;
		char outname[sizeof(fname)+12];

		if (fseek(infile,table_offset+4+i*4,SEEK_SET)<0) return 1;
		if (fread(buf,1,4,infile)!=4) return 1;
		wav_entry_offset = read32(buf)+offset;

		/* go to the entry */
		if (fseek(infile,wav_entry_offset,SEEK_SET)<0) return 1;

		/* contains AFC type:
		    0 = type 9 (9 byte frames, samples from nibbles)
		    1 = type 5 (5 byte frames, samples from half-nibbles)
		    3 = not sure, but seems to decode OK */
		if (fread(buf,1,4,infile)!=4) return 1;
		if (buf[1]==1) type=5;
		else type=9;

		/* contains srate */
		if (fread(buf,1,4,infile)!=4) return 1;
		srate=((buf[1]<<8) | buf[2])/2;
		if (type==5) srate=32000;	/* hack - not sure whether this is true generally */

		/* offset */
		if (fread(buf,1,4,infile)!=4) return 1;
		afcoffset=read32(buf);

		/* size */
		if (fread(buf,1,4,infile)!=4) return 1;
		afcsize=read32(buf);

		if (verbose) {
			printf("offset\t%x\tsize\t%x\tsrate\t%d\ttype\t%d\n",afcoffset,afcsize,srate,type);
		}
                
		sprintf(outname,"%s_%08x.wav",fname,i);
		if (dumpAFC(awfile,afcoffset,afcsize,srate,type,outname)) return 1;
	}

	if (fclose(awfile)==EOF) return 1;

	if (fseek(infile,next_aw_offset,SEEK_SET)<0) return 1;

        return 0;
}

int doWSYS(FILE * infile, const int offset) {
	unsigned char buf[4];
	int WINFoffset;
        int aw_count;
	int old_offset;
	int i;

	old_offset = ftell(infile);
	if (old_offset<0) return 1;

	if (fseek(infile,offset,SEEK_SET)<0) return 1;

	/* WSYS tag */
	if (fread(buf,1,4,infile)!=4) return 1;
	if (memcmp(buf,"WSYS",4)) {
		fprintf(stderr,"WSYS file expected at 0x%x\n",offset);
		return 1;
	}

	if (fseek(infile,12,SEEK_CUR)<0) return 1; /* skip stuff I don't use */

	/* offset of WINF */
	if (fread(buf,1,4,infile)!=4) return 1;
	WINFoffset = read32(buf) + offset;

	if (fseek(infile,WINFoffset,SEEK_SET)<0) return 1;

	/* WINF tag */
	if (fread(buf,1,4,infile)!=4) return 1;
	if (memcmp(buf,"WINF",4)) {
		fprintf(stderr,"expected WINF tag at 0x%x\n",WINFoffset);
		return 1;
	}

	/* number of .aw files to decode */
	if (fread(buf,1,4,infile)!=4) return 1;
        aw_count = read32(buf);

        for (i=0;i<aw_count;i++) {
		if (doaw(infile,offset)) {
			fprintf(stderr, "error parsing .aw file\n");
			return 1;
		}
        }

	if (fseek(infile,old_offset,SEEK_SET)<0) return 1;

	return 0;
}


int main(int argc, char ** argv) {
	FILE * infile = NULL;
	unsigned char buf[4];
	int badstuff=0;
	int chunksdone=0;
	const char infilename[] = "JaiInit.aaf";
	int i;
       
	infile = fopen(infilename,"rb");

	printf("wwdumpsnd 0.4 by hcs\ndump audio from Wind Waker or Super Mario Sunshine\nneeds JaiInit.aaf and *.aw in current directory\n(if Sunshine, the file is 'msound.aaf', from 'nintendo.szs',\nbut you'll need to rename it :))\n\n");

	for (i=1;i<argc;i++) {
		if (!strcmp("-v",argv[i])) verbose=1;
		else {
			printf("usage: %s [-v]\n",argv[0]);
			return 1;
		}
	}

	if (!infile) {
		fprintf(stderr,"failed to open %s\n",infilename);
		return 1;
	}

	if (!verbose) printf("working...\n");

	/* read header (chunk descriptions) */
	while (!feof(infile) && !badstuff && !chunksdone) {
		int chunkid,offset,size,id;
		fread(buf,4,1,infile);

		chunkid = read32(buf);

		switch (chunkid) {
			case 1:
			case 5:
			case 6:
			case 7:
				if (verbose) {
					printf("%d:\t",chunkid);
				}

				fread(buf,4,1,infile);
				offset=read32(buf);
				if (verbose) {
					printf("offset\t%08x\t",offset);
				}

				fread(buf,4,1,infile);
				size=read32(buf);
				if (verbose) {
					printf("size\t%08x\t",size);
				}

				/* maybe continue if this != 0 ? */
				fread(buf,4,1,infile);

				if (verbose) {
					printf("%08x",read32(buf));

					printf("\n");
				}
				break;
			case 2:
			case 3:
				while (!feof(infile) && !badstuff) {
					fread(buf,4,1,infile);
					offset=read32(buf);
					if (offset!=0) {
						if (verbose) {
							printf("%d:\toffset\t%08x\t",chunkid,offset);
						}
					} else break;

					fread(buf,4,1,infile);
					size=read32(buf);
					if (verbose) {
						printf("size\t%08x\t",size);
					}

					fread(buf,4,1,infile);
					id=read32(buf);
					if (verbose) {
						printf("id\t%08x",id);

						printf("\n");
					}

					if (chunkid==3 && doWSYS(infile,offset)) {
						fprintf(stderr,"dump failed\n");
						badstuff=1;
					}
				}

				break;
			case 0:
				chunksdone=1;
				break;
			default:
				fprintf(stderr,"unknown id 0x%x\n",chunkid);
				badstuff=1;
				break;
		}
	}

	if (feof(infile)) {
		fprintf(stderr,"end of file encountered while trying to read chunk layout\n");
		fclose(infile); infile = NULL;
		return 1;
	}
	if (badstuff) {
		fclose(infile); infile = NULL;
		return 1;
	}

	if (verbose)
		printf("end of chunks at 0x%x\n",ftell(infile));

	fclose(infile); infile = NULL;
}