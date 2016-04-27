#include <stdio.h>
#include <stdint.h>

/* FJFU, list subfile offsets in FJF format files in Saint Seiya The Hades */

struct fjfhead_s {
    uint8_t magic[12]; /* "FJF\0",2,0,0,0xc0,0x20,0,0,0 */
    uint8_t swdmoff[4]; /* 32-bit little endian */
    uint8_t totalsize[4]; /* whole file 32-bit little endian */
    uint8_t padding[12];

};

struct chunkhead_s {
    uint8_t name[4]; /* "seds", "swdm" */
    uint8_t unk1[4]; /* all zero */
    uint8_t size[4]; /* 32-bit little endian */
    uint8_t unk2[4];
    uint8_t unk3[16];
    uint8_t shortname[16]; /* source file name, truncated to 15 chars? */
};

/*
struct sedschunk {
};
*/

struct swdmhead_s {
    uint8_t unk1[8];
    uint8_t entries[4]; /* 32-bit little endian */
    uint8_t unk2[4];
    uint8_t unk3[0x18];
    uint8_t tabsize2[4]; /* + 0x10 */
    uint8_t tabsize[4]; 
    /* followed by a table of swdmentry */
};

struct swdmentry_s {
    uint8_t offset[4];
    uint8_t unk1[4]; /* always 0x10? */
    uint8_t unk2[2]; /* always 0x7f0f? */
    uint8_t name[0x16]; /* null terminated, UTF-8? */
};

uint32_t get32bit(uint8_t buf[]) {
    return (buf[0]) | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
}

int main(int argc, char ** argv) {
    FILE * infile;
    struct fjfhead_s fjfhead;
    struct chunkhead_s chunkhead;
    struct swdmhead_s swdmhead;
    struct swdmentry_s swdmentry;
    int i;
    int tabstart;
    if (argc!=2) {fprintf(stderr,"usage: %s SE.BIN\n",argv[0]); return 1;}


    infile=fopen(argv[1],"rb");
    if (!infile) {fprintf(stderr,"error opening %s\n",argv[1]); return 1;}

    fread(&fjfhead,sizeof(fjfhead),1,infile);

    if (memcmp(fjfhead.magic,"FJF\0\x2\0\0\xc0\x20\0\0\0",8)) {
	fprintf(stderr,"unrecgonized header\n");
	return 1;
    }

    while (fread(&chunkhead,sizeof(chunkhead),1,infile)==1) {
	if (!memcmp(chunkhead.name,"seds",4)) {
	    /* ignore for now */
	} else if (!memcmp(chunkhead.name,"swdm",4)) {
	    break;
	} else {
	    fprintf(stderr,"unknown chunk %c%c%c%c\n",chunkhead.name[0],chunkhead.name[1],chunkhead.name[2],chunkhead.name[3]);
	    return 1;
	}

	fseek(infile,get32bit(chunkhead.size)-sizeof(chunkhead),SEEK_CUR);
    }
    if (memcmp(chunkhead.name,"swdm",4)) {
	fprintf(stderr,"swdm chunk not found\n");
	return 1;
    }

    fread(&swdmhead,sizeof(swdmhead),1,infile);
    tabstart=ftell(infile);
    for (i=0;i<get32bit(swdmhead.entries);i++) {
	fread(&swdmentry,sizeof(swdmentry),1,infile);
	printf("mfaudio \"%s\" /IF44100 /IC1 /IH%x /OTWAVU \"%s%d.wav\"\n",argv[1],0x20+tabstart+get32bit(swdmhead.tabsize2)+get32bit(swdmentry.offset),argv[1],i);
    }

    return 0;
}
