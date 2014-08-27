#include <stdio.h>
#include <string.h>

/*
   SPTex 0.0
   extract DSPs (ADPCM) from SPT/SPD, ignores PCM (until I see one I don't
          know how to treat it)

   by hcs
*/

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

// make 16-bit big endian value
void make16bit(unsigned char* p, unsigned short i) {
    p[1]=i;
    p[0]=i>>8;
}

// make 32-bit big endian value
void make32bit(unsigned char* p, unsigned long i) {
    p[3]=i;
    p[2]=i>>8;
    p[1]=i>>16;
    p[0]=i>>24;
}

int main(int argc, char ** argv) {
    FILE * sptfile, * spdfile, * outfile;
    int filecount,i,j;
    int part1idx; // part1=type, freq, loop points, size
    int part2idx; // part2=coeffs, gain, init p/s, yn1, yn2
    int part3idx; //
    int part4idx; // 
    int dataoff,nextdataoff;
    int type;
    char outbuf[4],buf1[0x1C],buf2[0x2E];
    char typename[30], namebase[513],*t,fname[513];
    
    printf("SPTex 0.0\n\n");

    if (argc!=3) {printf("usage: sptex bank.spt bank.spd\n"); return 1;}

    if (!(sptfile=fopen(argv[1],"rb"))) {printf("failed to open %s\n",argv[1]); return 1;}
    if (!(spdfile=fopen(argv[2],"rb"))) {printf("failed to open %s\n",argv[2]); return 1;}

    // generate namebase
    t=strrchr(argv[1],'\\');
    if (!t) t=argv[1];
    else t++;
    for (i=0;t<strrchr(argv[1],'.');t++,i++) namebase[i]=*t;
    namebase[i]='\0';

    // get count
    fread(buf1,1,4,sptfile);
    filecount = get32bit(buf1);

    printf("%d entries\n\n",filecount);

    dataoff=0;

    part1idx=4;
    part2idx=part1idx+filecount*0x1c;
    //part3idx=part2idx+filecount*0x2e;
    //part4idx=part3idx+filecount*8;

    for (i=0;i<filecount;i++) {
    //for (i=0;i<10;i++) {

        fseek(sptfile,part1idx,SEEK_SET);
        fread(buf1,1,0x1c,sptfile);

        fseek(sptfile,part2idx,SEEK_SET);
        fread(buf2,1,0x2e,sptfile);

        type=get32bit(buf1);

        if (type&1) strcpy(typename,"looped ");
        else strcpy(typename,"");

        switch(type) {
            case 0:
            case 1:
                strcat(typename,"ADPCM");
                break;
            case 2:
            case 3:
                strcat(typename,"16 bit PCM");
                break;
            case 4:
            case 5:
                strcat(typename,"8 bit PCM");
                break;
            default:
                printf("unknown type in file %d\n",i);
                return 1;
        }

        nextdataoff=get32bit(buf1+0x10)/2+1;
        printf("file %d\n\ttype:\t%s\n\tfreq:\t%d\n\toffset:\t%#x\n\tsize\t%#x (%d)\n",i,typename,get32bit(buf1+4),dataoff,nextdataoff-dataoff,nextdataoff-dataoff);

        // generate DSP

        sprintf(fname,"%s%03d.dsp",namebase,i);

        if (!(type&(~1))) {

        if (!(outfile=fopen(fname,"wb"))) {printf("error opening %s\n",fname); return 1;}

        // write number of samples
        make32bit(outbuf,(nextdataoff-dataoff)*7/4);
        fwrite(outbuf,1,4,outfile);

        // write number of nibbles
        make32bit(outbuf,(nextdataoff-dataoff)*2);
        fwrite(outbuf,1,4,outfile);

        // write srate
        fwrite(buf1+4,1,4,outfile);

        // loop
        make16bit(outbuf,type&1);
        fwrite(outbuf,1,2,outfile);

        // format
        make16bit(outbuf,0);
        fwrite(outbuf,1,2,outfile);

        // loop start offset
        make32bit(outbuf,get32bit(buf1+8)-dataoff*2);
        fwrite(outbuf,1,4,outfile);

        // loop end offset
        make32bit(outbuf,get32bit(buf1+12)-dataoff*2);
        fwrite(outbuf,1,4,outfile);

        // "current address"
        make32bit(outbuf,2);
        fwrite(outbuf,1,4,outfile);

        // coeffs, gain, init predictor/scale, loop too?
        fwrite(buf2,1,0x2e,outfile);

        // padding

        fseek(outfile,0x60,SEEK_SET);

        // ADPCM data
        fseek(spdfile,dataoff,SEEK_SET);

        for (j=dataoff;j<nextdataoff;j++) {
            fread(outbuf,1,1,spdfile);
            fwrite(outbuf,1,1,outfile);
        }

        fclose(outfile);
        
        } else printf("not ADPCM, skipping\n");

        printf("\n");

        dataoff=(nextdataoff+7)/8*8;

        part1idx+=0x1c;
        part2idx+=0x2e;
    }

    fclose(sptfile);
    fclose(spdfile);

    return 0;
}
