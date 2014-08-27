#include <stdio.h>

// adxloop 0.1
// force an ADX (2 channel) to loop
// by hcs

int samplestooffset(int s) {
    return s/32*36-3;
}
int samplestosize(int s) {
    return (s+0x1e)/0x20*0x24;
}

unsigned int get32bit(unsigned char * b) {
    return (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];
}

void make32bit(unsigned int i, unsigned char * b) {
    b[0] = i >> 24;
    b[1] = i >> 16;
    b[2] = i >> 8;
    b[3] = i;
}

int main(int argc, char ** argv) {
    char buf[0x20];
    int startoff, length;
    int introstartoff, introlength = 0;
    FILE * infile, * introfile = NULL, * outfile;
    int i;

    printf("adxloop 0.1 by hcs\n");

    if (argc != 3 && argc != 4) {
    printf("usage: %s [intro.adx] loop.adx out.adx\n",argv[0]); return 1;}

    if (argc == 4) {
        introfile = fopen(argv[1],"rb");
        if (!introfile) {printf("error opening %s\n",argv[1]); return 1;}
        infile = fopen(argv[2],"rb");
        if (!infile) {printf("error opening %s\n",argv[2]); return 1;}
        outfile = fopen(argv[3],"wb");
        if (!outfile) {printf("error opening %s\n",argv[3]); return 1;}

        fread(buf,1,0x18,introfile); // read intro header

        introstartoff = get32bit(buf+0)&0xffff;
        introlength = get32bit(buf+0x0c);
    } else {
        infile = fopen(argv[1],"rb");
        if (!infile) {printf("error opening %s\n",argv[1]); return 1;}
        outfile = fopen(argv[2],"wb");
        if (!outfile) {printf("error opening %s\n",argv[2]); return 1;}
    }

    printf("writing new header\n");

    fread(buf,1,0x18,infile); // read existing header

    startoff = get32bit(buf+0)&0xffff;
    length = get32bit(buf+0x0c);
    make32bit(introlength+length,buf+0xc); // new length
    make32bit(0x800000EC,buf+0); // change offset (to make room for loop data)
    make32bit(0x01F40300,buf+0x10); // set type to most commonly supported
    make32bit(0x000B0001,buf+0x14); // needed for CinePak to loop
    fwrite(buf,1,0x18,outfile); // write beginning of header

    make32bit(1,buf); // loop flag
    make32bit(introlength/32*32,buf+4); // start address (samples)
    make32bit(samplestooffset(0xf0+introlength),buf+8); // start address (offset)
    make32bit(introlength+length,buf+12); // end address (samples)
    make32bit(samplestooffset(introlength+length),buf+16);

    fwrite(buf,1,0x14,outfile); // new header data

    printf("copying data\n");

    fseek(outfile,0xf0-6,SEEK_SET);
    fwrite("(c)CRI",6,1,outfile);

    if (introfile) {
        fseek(introfile,introstartoff+4,SEEK_SET);
        for (i=samplestosize(introlength);i>0;i--) {
            fread(buf,1,1,introfile);
            fwrite(buf,1,1,outfile);
        }

        fclose(introfile);
    }
    fseek(infile,startoff+4,SEEK_SET);
    while (fread(buf,1,1,infile)==1) fwrite(buf,1,1,outfile);

    fclose(infile);
    fclose(outfile);

    return 0;
}
