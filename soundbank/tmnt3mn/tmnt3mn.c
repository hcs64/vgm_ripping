#include <stdio.h>
#include <string.h>

// TMNT3MN extractor 0.1
// by hcs

const int headsize=0x100;
const int interleave=0x100;


// get 16-bit big endian value
unsigned int get16bit(unsigned char* p)
{
    return (p[0] << 8) | p[1];
}

unsigned int get32bit(unsigned char* p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

void make32bit(unsigned int in, unsigned char * p) {
    p[0]=(in>>24)&0xff;
    p[1]=(in>>16)&0xff;
    p[2]=(in>>8)&0xff;
    p[3]=in&0xff;
}

int main(void) {
    FILE * infile, * outfileL, * outfileR;
    int i,j;
    unsigned int searchoffset,offset,DSPcount,DSPsize;
    unsigned int srate;
    char *t,namebase[257]="tmnt3mn",fname[257+5];
    unsigned char buf[0x40];

    printf("TMNT3MN extractor 0.1\nextract DSP files from strbgm.bin in TMNT3 \"Mutant Nightmare\"\n\n");

    infile = fopen("strbgm.bin","rb");
    if (!infile) {printf("error opening strbgm.bin\n"); return 1;}

    searchoffset=0;
    DSPcount=0;
    while (!feof(infile) && fread(buf,1,4,infile)==4) {
        DSPsize = get32bit(buf);
        fread(buf,1,4,infile);
        srate=get32bit(buf);

        // sometimes for no good reason...
        if (srate!=32000) {
            searchoffset+=0x800;
            fseek(infile,searchoffset,SEEK_SET);
            continue;
        }

        printf("%04d:%08x\tsize %08x\tfreq %d\n",DSPcount,searchoffset,DSPsize,get32bit(buf));

        // create output file (Left)
        sprintf(fname,"%s%04dL.dsp",namebase,DSPcount);
        outfileL=fopen(fname,"wb");
        if (!outfileL) {printf("error opening %s\n",fname); return 1;}

        // construct a header (Left)
        // num samples
        make32bit((DSPsize-headsize)*14/8/2,buf);
        fwrite(buf,1,4,outfileL);
        // nibbles
        make32bit((DSPsize-headsize)/2,buf);
        fwrite(buf,1,4,outfileL);
        // sample rate
        make32bit(srate,buf);
        fwrite(buf,1,4,outfileL);

        fseek(infile,searchoffset+0x80,SEEK_SET);
        fread(buf,1,0x40,infile);
        // loop flag, format
        fwrite(buf,1,4,outfileL);
        // loop start
        make32bit(get32bit(buf+4),buf); // double usage of buf is OK here
        fwrite(buf,1,4,outfileL);
        // loop end
        make32bit(get32bit(buf+8),buf); // and here
        fwrite(buf,1,4,outfileL);
        // ca
        fwrite(buf+12,1,4,outfileL);
        // decode coeffs, etc.
        fwrite(buf+16,1,0x20,outfileL);

        // filler
        memset(buf,0,36);
        fwrite(buf,1,36,outfileL);
        
        // create output file (Right)
        sprintf(fname,"%s%04dR.dsp",namebase,DSPcount);
        outfileR=fopen(fname,"wb");
        if (!outfileR) {printf("error opening %s\n",fname); return 1;}

        // construct a header
        // num samples
        make32bit((DSPsize-headsize)*14/8/2,buf);
        fwrite(buf,1,4,outfileR);
        // nibbles
        make32bit((DSPsize-headsize)/2,buf);
        fwrite(buf,1,4,outfileR);
        // sample rate
        make32bit(srate,buf);
        fwrite(buf,1,4,outfileR);

        fseek(infile,searchoffset+0xC0,SEEK_SET);
        fread(buf,1,0x40,infile);
        // loop flag, format
        fwrite(buf,1,4,outfileR);
        // loop start
        make32bit(get32bit(buf+4),buf); // double usage of buf is OK here
        fwrite(buf,1,4,outfileR);
        // loop end
        make32bit(get32bit(buf+8),buf); // and here
        fwrite(buf,1,4,outfileR);
        // ca
        fwrite(buf+12,1,4,outfileR);
        // decode coeffs
        fwrite(buf+16,1,0x20,outfileR);

        // filler
        memset(buf,0,36);
        fwrite(buf,1,36,outfileR);

        fseek(infile,searchoffset+headsize,SEEK_SET);

        // copy data (deinterleave by 0x100)
        for (j=0;j<(DSPsize-headsize);j+=interleave*2) {
            for (i=0;i<interleave;i+=8) {
                fread(buf,1,8,infile);
                fwrite(buf,1,8,outfileL);
            }
            for (i=0;i<interleave;i+=8) {
                fread(buf,1,8,infile);
                fwrite(buf,1,8,outfileR);
            }
        }

        fclose(outfileL);
        fclose(outfileR);
        outfileL=outfileR=NULL;

        DSPcount++;

        searchoffset=(searchoffset+DSPsize+0xfff)/0x800*0x800;
        fseek(infile,searchoffset,SEEK_SET);

        //break;
    }

    fclose(infile);

    return 0;
}
