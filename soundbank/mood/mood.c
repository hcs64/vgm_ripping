#include <stdio.h>

// MOOd (MOO depacker) 0.0
// by hcs
// for .MOO files from Robotech: Battlecry

// get 16-bit big endian value
int get16bit(unsigned char* p)
{
    return (p[0] << 8) | p[1];
}

// get 32-bit big endian value
int get32bit(unsigned char* p)
{
   return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

// make 32-bit big endian value
void make32bit(unsigned char* p, unsigned long i) {
    p[3]=i;
    p[2]=i>>8;
    p[1]=i>>16;
    p[0]=i>>24;
}

int main(int argc, char ** argv) {
    FILE * infile, * outfile;
    char namebase[257],fname[257], *t,tc;
    char buf[32];
    int start,count,i,j;
    unsigned long sa,ea;

    printf("MOOd (MOO depacker) 0.0\n");

    if (argc!=2) {printf("usage: %s file.moo\n",argv[0]); return 1;}

    infile = fopen(argv[1],"rb");

    if (!infile) {printf("error opening %s\n",argv[1]); return 1;}

    // get namebase
    t=(char*)strrchr(argv[1],'\\');
    if (!t) t=argv[1];
    else t++;
    for (i=0;t<(char*)strrchr(argv[1],'.');t++,i++) namebase[i]=*t;
    namebase[i]='\0';

    // typical values, excpet for short moo
    start=0x480;
    count=0x18;

    // check for short moo
    // 1. is long at 0x60 == 0?
    fseek(infile,0x60,SEEK_SET);
    fread(buf,1,4,infile);
    if (*((unsigned long*)buf)==0) {
        // 2. if [0x70] == initial predictor/scale for first stream
        fseek(infile,0x70,SEEK_SET);
        fread(buf,1,1,infile);
        fseek(infile,0x2b,SEEK_SET);
        fread(buf+1,1,1,infile);

        if (buf[0]==buf[1]) {
            printf("\nsmall MOO detected\n\n");
            start=0x70;
            count=0x2;
        }
    }

    // extract
    for (i=0;i<count;i++) {
        fseek(infile,0x30*i,SEEK_SET);

        // stereo is assumed
        if (i%2) tc='R';
        else tc='L';
        sprintf(fname,"%s%02d%c.dsp",namebase,i/2,tc);
        printf ("%s ",fname);
        outfile = fopen(fname,"wb");
        if (!outfile) {printf("\nerror opening %s\n",fname); return 1;}

        // read loop start address
        fread(buf,1,4,infile);
        sa=get32bit(buf);
        // read loop end address
        fread(buf,1,4,infile);
        ea=get32bit(buf);

        // write sample count
        make32bit(buf,(ea-sa)*7/8);
        fwrite(buf,1,4,outfile);

        // write nibble count
        make32bit(buf,ea-sa);
        fwrite(buf,1,4,outfile);

        // write srate
        make32bit(buf,32000); // 32KHz assumed
        fwrite(buf,1,4,outfile);

        // write loop flag, format
        if (count==2) make32bit(buf,0x00010000);
        else make32bit(buf,0);
        fwrite(buf,1,4,outfile);

        // write loop start offset
        make32bit(buf,2);
        fwrite(buf,1,4,outfile);

        // write loop end offset
        make32bit(buf,ea-sa+2);
        fwrite(buf,1,4,outfile);

        // write ca
        make32bit(buf,0);
        fwrite(buf,1,4,outfile);

        // copy decode coeffs
        fread(buf,1,32,infile);
        fwrite(buf,1,32,outfile);

        // write gain
        make32bit(buf,0);
        fwrite(buf,1,2,outfile);

        // copy initial predictor/scale
        fread(buf,1,4,infile);
        fwrite(buf+2,1,2,outfile);

        // write samp hist (junk)
        fwrite(buf,1,2,outfile);
        fwrite(buf,1,2,outfile);

        // copy initial predictor/scale again (loop)
        fwrite(buf+2,1,2,outfile);

        // write samp hist (junk)
        fwrite(buf,1,2,outfile);
        fwrite(buf,1,2,outfile);

        // filler
        memset(buf,0,22);
        fwrite(buf,1,22,outfile);

        printf(" %08x\tsize %d\n",(sa-2)/2,(ea-sa)/2);

        // write t3h d4t4z
        fseek(infile,start+(sa-2)/2,SEEK_SET);
        for (j=0;j<(ea-sa)/2;j++) {
            fread(buf,1,1,infile);
            fwrite(buf,1,1,outfile);
        }

        fclose(outfile);
    }

    return 0;
}
