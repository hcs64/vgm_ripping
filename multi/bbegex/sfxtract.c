#include <stdio.h>
#include <string.h>

// sfxtract 0.0
// extract Gamecube DSPs from SFXs found in Batman Begins (and others)
// by hcs

// kudos to Eurocom for coming up with such an easy to parse format

int get32bit(unsigned char* p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

int main(int argc, char ** argv) {
    FILE * infile, * outfile;
    int i,dspcount,j;
    int secoff[4],dataoff,datasize,headeroff;
    unsigned char buf[0x60];
    char *t,namebase[257],fname[257+4];

    printf("sfxtract 0.0\nextract gamecube DSPs from SFXs\n\n");

    if (argc != 2) {printf("usage: %s archive.sfx\n",argv[0]); return 1;}

    infile = fopen(argv[1],"rb");
    if (!infile) {printf("error opening %s\n",argv[1]); return 1;}

    // generate namebase
    t=strrchr(argv[1],'\\');
    if (!t) t=argv[1];
    else t++;
    for (i=0;t<strrchr(argv[1],'.');t++,i++) namebase[i]=*t;
    namebase[i]='\0';

    fread(buf,1,0x40,infile);

    if (memcmp(buf,"MUSX",4) || memcmp(buf+0x10,"GC__",4) || memcmp(buf+0x16,"\x05\x0a",2)) {printf("unrecognized format\n"); return 1;}

    secoff[0]=get32bit(buf+0x20);   // ignore first section
    secoff[1]=get32bit(buf+0x28);   // DSP entries
    secoff[2]=get32bit(buf+0x30);   // DSP headers
    secoff[3]=get32bit(buf+0x38);   // DSP data

    fseek(infile,secoff[1],SEEK_SET);

    fread(buf,1,4,infile);
    dspcount = get32bit(buf);

    secoff[1]+=4;

    for (i=0;i<dspcount;i++) {
        // DSP entry
        fread(buf,1,0x20,infile);

        dataoff=get32bit(buf+0x04);
        datasize=get32bit(buf+0x08); // 0x10 is a bit smaller
        headeroff=get32bit(buf+0x14);

        printf("DSP #%d\n\tData offset:\t%08x\n\tData size:\t%08x\n\tHeader offset:\t%08x\n",i,dataoff,datasize,headeroff);

        sprintf(fname,"%s%03d.dsp",namebase,i);
        outfile=fopen(fname,"wb");
        if (!outfile) {printf("error opening %s\n",fname); return 1;}

        fseek(infile,secoff[2]+headeroff,SEEK_SET);

        fread(buf,1,0x60,infile);
        fwrite(buf,1,0x60,outfile);

        fseek(infile,secoff[3]+dataoff,SEEK_SET);
        
        for (j=0;j<datasize;j++) {
            fread(buf,1,1,infile);
            fwrite(buf,1,1,outfile);
        }

        fclose(outfile);

        secoff[1]+=0x20;
        fseek(infile,secoff[1],SEEK_SET);
    }

    fclose(infile);

    return 0;
}
