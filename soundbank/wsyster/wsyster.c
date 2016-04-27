/*
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int verbose = 1;

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
void write16le(int in, unsigned char * buf) {
	buf[0]=in&0xff;
	buf[1]=(in>>8)&0xff;
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

void AFCdecodebuffer
(
 unsigned char *input, /* location of encoded source samples */
 signed short *out,   /* location of destination buffer (16 bits / sample) */
 short * histp,
 short * hist2p
)
{
	int sample;
	short nibbles[16];
	int i,j;
	char *src,*dst;
	short idx;
	short delta;
	short hist=*histp;
	short hist2=*hist2p;

	dst = (char*)out;

	src=input;
	delta = 1<<(((*src)>>4)&0xf);
	idx = (*src)&0xf;

	src++;
    
	for(i = 0; i < 16; i = i + 2) {
		j = ( *src & 255) >> 4;
		nibbles[i] = j;
		j = *src & 255 & 15;
		nibbles[i+1] = j;
		src++;
	}

	for(i = 0; i < 16; i = i + 1) {
		if(nibbles[i] >= 8) 
			nibbles[i] = nibbles[i] - 16;
	}
     
	for(i = 0; i<16 ; i = i + 1) {

		sample = (delta * nibbles[i])<<11;
		sample += ((long)hist * afccoef[idx][0]) + ((long)hist2 * afccoef[idx][1]);
		sample = sample >> 11;

		if(sample > 32767) {
			sample = 32767;
		}
		if(sample < -32768) {
			sample = -32768;
		}

		*(short*)dst = (short)sample;
		dst = dst + 2;

		hist2 = hist;
		hist = (short)sample;
        
	}

	*histp=hist;
	*hist2p=hist2;

	return;
}

/* dump a WAV, decoding AFC */
/* return 0 on success, 1 on failure */
int dumpAFC(FILE * const infile, const int offset, const int size, const int channels, const int srate, const char * const filename) {
	long oldpos;
	FILE * outfile;
	int sizeleft;
	int outsize,outsizetotal;
    int curchan;
	
	unsigned char wavhead[44] = {
		0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,  0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
		0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61,  0x00, 0x00, 0x00, 0x00
	};

	oldpos = ftell(infile);
	if (oldpos < 0) return 1;

	outfile = fopen(filename,"wb");
	if (!outfile) return 1;

	outsize = size/9*16*2*channels;
	outsizetotal = outsize+8;
	write32le(outsizetotal,wavhead+4);
	write32le(outsize,wavhead+40);
    write16le(channels,wavhead+0x16);
	write32le(srate,wavhead+24);
	write32le(srate*2*channels,wavhead+28);
	if (fwrite(wavhead,1,44,outfile)!=44) return 1;

	if (fseek(infile,offset,SEEK_SET)<0) return 1;

    struct {
	    unsigned char inbuf[9];
        short outbuf[16];
        short hist,hist2;
    } * chinfo;

    chinfo = calloc(channels, sizeof(*chinfo));

    curchan = 0;
    
	for (sizeleft=size;sizeleft>=9;sizeleft-=9) {
        unsigned char * inbuf = chinfo[curchan].inbuf;
        short * outbuf = chinfo[curchan].outbuf;
        short * histp = &chinfo[curchan].hist;
        short * hist2p = &chinfo[curchan].hist2;

		if (fread(inbuf,1,9,infile) != 9)
			return 1;

		AFCdecodebuffer(inbuf,outbuf,histp,hist2p);

        curchan = (curchan + 1) % channels;
        if (curchan == 0) {
            int c, i;
            for (c = 0; c < channels; c++) {
                for (i = 0; i < 16; i++) {
		            if (fwrite(&chinfo[c].outbuf[i],1,2,outfile) != 2)
                        return 1;
                }
            }
        }
	}

    free(chinfo);

	if (fclose(outfile)==EOF) return 1;

	if (fseek(infile,oldpos,SEEK_SET)<0) return 1;

	return 0;
}

int dumpPCM8(FILE * const infile, const int offset, const int size, const int channels, const int srate, const char * const filename) {
	long oldpos;
	FILE * outfile;
	int sizeleft;
	int outsize,outsizetotal;
    int curchan;
    unsigned char inbuf[BUFSIZ];
	
	unsigned char wavhead[44] = {
		0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,  0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
		0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x08, 0x00, 0x64, 0x61, 0x74, 0x61,  0x00, 0x00, 0x00, 0x00
	};

	oldpos = ftell(infile);
	if (oldpos < 0) return 1;

	outfile = fopen(filename,"wb");
	if (!outfile) return 1;

	outsize = size*channels;
	outsizetotal = outsize+8;
	write32le(outsizetotal,wavhead+4);
	write32le(outsize,wavhead+0x28);
    write16le(channels,wavhead+0x16);
	write32le(srate,wavhead+0x18);
	write32le(srate*channels,wavhead+0x1C);
	if (fwrite(wavhead,1,44,outfile)!=44) return 1;

	if (fseek(infile,offset,SEEK_SET)<0) return 1;

	for (sizeleft=size;sizeleft>=BUFSIZ;sizeleft-=BUFSIZ) {
        int i;

		if (fread(inbuf,1,BUFSIZ,infile) != BUFSIZ)
			return 1;

        for (i = 0; i < BUFSIZ; i++) {
            inbuf[i] += 0x80;
        }

		if (fwrite(inbuf,1,BUFSIZ,outfile) != BUFSIZ)
            return 1;
	}

    if (sizeleft > 0) {
        int i;

		if (fread(inbuf,1,sizeleft,infile) != sizeleft)
			return 1;

        for (i = 0; i < sizeleft; i++) {
            inbuf[i] += 0x80;
        }

		if (fwrite(inbuf,1,sizeleft,outfile) != sizeleft)
            return 1;
    }

	if (fclose(outfile)==EOF) return 1;

	if (fseek(infile,oldpos,SEEK_SET)<0) return 1;

	return 0;
}


int doWSYS(FILE * infile, const int offset) {
	FILE * awfile;
	unsigned char buf[4];
	char fname[113]={0};
	int WINFoffset;
	int aw_count;
	int aw_name;
	int table_offset;
	int wav_count;
	int old_offset;
	int i;
	int aw_i;

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

	/* aw count */
	if (fread(buf,1,4,infile)!=4) return 1;
	aw_count=read32(buf);
	if (verbose) {
		printf("%d aw entries\n",aw_count);
	}

	for (aw_i=0;aw_i<aw_count;aw_i++) {
		if(fseek(infile,WINFoffset+8+(aw_i*4),SEEK_SET)<0) return 1;

		/* offset to list of wave table entry offsets */
		if (fread(buf,1,4,infile)!=4) return 1;
		aw_name = read32(buf) + offset;
		table_offset = aw_name+112;

		if (fseek(infile,aw_name,SEEK_SET)<0) return 1;

		/* aw file name */
		if (fread(fname,1,112,infile)!=112) return 1;
		awfile = fopen(fname,"rb");

		/* aw file may not exist, allow this to slide */
		if (!awfile) {
			fprintf(stderr,"%s not found\n",fname);
			continue;
		}

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
            unsigned unk1, unk2, unk3;
            int channels = 1;
            int pcm;

			if (fseek(infile,table_offset+4+i*4,SEEK_SET)<0) return 1;
			if (fread(buf,1,4,infile)!=4) return 1;
			wav_entry_offset = read32(buf)+offset;

			/* go to the entry */
			if (fseek(infile,wav_entry_offset,SEEK_SET)<0) return 1;

			/* unknown */
			if (fread(buf,1,4,infile)!=4) return 1;
            unk1 = read32(buf);
            if (buf[1] == 0x02) {
                pcm = 1;
            } else {
                pcm = 0;
            }

			/* contains srate */
			if (fread(buf,1,4,infile)!=4) return 1;
			srate=((buf[1]<<8) | buf[2])/2;
            unk3 = read32(buf);

			/* offset */
			if (fread(buf,1,4,infile)!=4) return 1;
			afcoffset=read32(buf);

			/* size */
			if (fread(buf,1,4,infile)!=4) return 1;
			afcsize=read32(buf);

			/* unknown (loop point?) */
			if (fread(buf,1,4,infile)!=4) return 1;
            unk2 = read32(buf);

			if (verbose) {
				printf("index\t\%08x\toffset\t%x\tsize\t%x\tsrate\t%d\n",i,afcoffset,afcsize,srate);
			}

			sprintf(outname,"%s_%08x.wav",fname,i);
            if (pcm) {
			    if (dumpPCM8(awfile,afcoffset,afcsize,channels,srate,outname)) return 1;
            } else {
                if (dumpAFC(awfile,afcoffset,afcsize,channels,srate,outname)) return 1;
            }
		}

		if (fclose(awfile)==EOF) return 1;
	}

	if (fseek(infile,old_offset,SEEK_SET)<0) return 1;

	return 0;
}


int main(int argc, char ** argv) {
	FILE * infile = NULL;
	unsigned char buf[4];
	int i;

	if (argc!=2) {
		fprintf(stderr,"one argument: something.wsys\n");
		return 1;
	}
       
	infile = fopen(argv[1],"rb");
	if (!infile) {
		fprintf(stderr,"error opening %s\n",argv[1]);
		return 1;
	}

	doWSYS(infile,0);

	fclose(infile); infile = NULL;
}
