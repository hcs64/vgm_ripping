#include <stdio.h>
#include <string.h>
#include <malloc.h>

int read16(unsigned char * buf) {
    return (buf[0]<<8)|buf[1];
}
void write16(int in, unsigned char * buf) {
    buf[0]=in>>8;
    buf[1]=in&0xff;
}
int read32(unsigned char * buf) {
    return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
}

typedef struct {
    int start;
    int mult;
    int add;
    int type9;
    char * name;
} key;

#define KEY_COUNT 10
const key keys[KEY_COUNT]={
    /* I'm pretty sure this is right, based on a decrypted version of some GOD HAND tracks */
    {.start=0x49e1,.mult=0x4a57,.add=0x553d,.type9=0,.name="Clover Studio (GOD HAND, Okami)"},
    /* this is estimated */
    {.start=0x5f5d,.mult=0x58bd,.add=0x55ed,.type9=0,.name="Grasshopper Manufacture 0 (Blood+)"},
    /* this is estimated */
    {.start=0x50fb,.mult=0x5803,.add=0x5701,.type9=0,.name="Grasshopper Manufacture 1 (Killer7)"},
    /* this is estimated */
    {.start=0x4f3f,.mult=0x472f,.add=0x562f,.type9=0,.name="Grasshopper Manufacture 2 (Samurai Champloo)"},
    /* this is estimated */
    {.start=0x66f5,.mult=0x58bd,.add=0x4459,.type9=0,.name="Moss Ltd (Raiden III)"},
    /* this is estimated */
    {.start=0x5deb,.mult=0x5f27,.add=0x673f,.type9=0,.name="Sonic Team 0 (Phantasy Star Universe)"},
    /* this is estimated */
    {.start=0x46d3,.mult=0x5ced,.add=0x474d,.type9=0,.name="G.dev (Senko no Ronde)"},
    /* this seems to be dead on, but still estimated */
    {.start=0x440b,.mult=0x6539,.add=0x5723,.type9=0,.name="Sonic Team 1 (NiGHTS: Journey of Dreams)"},

    /* estimated, type 9 */
    {.start=0x07d2,.mult=0x1ec5,.add=0x0c7f,.type9=1,.name="Phantasy Star Online 2"},
    /* estimated, type 9 */
    {.start=0x0003,.mult=0x0d19,.add=0x043b,.type9=1,.name="Dragon Ball Z: Dokkan Battle"},
};

void guess_xor(int * scales, int scalecount, int startset, int startguess, int addset, int addguess, int multset, int multguess, int mask);
int guess_from_start(int * scales, int scalecount, int start, int minestimate, int mask);
int guess_from_low(int * scales, int scalecount, int start, int minestimate, int mask, int addguess, int multguess);
int guess_from_mult(int * scales, int scalecount, int start, int minestimate, int mask, int multguess);

void usage(const char * binname, int showkeys) {
    int i;
    printf("degod 0.5\n");
    if (!showkeys) {
        printf("usage: %s (-k n)|(-s x -m x -a x) -M [-b [-f n] [-n n] [-d 0|1]] [-o outfile.adx] [-e] infile.adx\n\n",binname);
        printf("Options:\n");
        printf("\t-k n\tspecify a key id (use -k ? for a list of keys)\n");
        printf("\t-s x -m x -a x\tspecify a key by components\n");
        printf("\t-M\tmask out unused high bits (cannot be undone)\n");
        printf("\t-b\tattempt to brute force a key\n");
        printf("\t-f n\tfirst frame to brute force from\n");
        printf("\t-n n\tnumber of frames to use in brutal attack\n");
        printf("\t-d 0|1\tdifferent keys per channel, do channel 0 or 1\n");
        printf("\t-o outfile.adx\toutput to a different file\n\t\t(default is to modify original)\n");
        printf("\t-e\tencrypt, rather than decrypt\n");
        printf("\tinfile.adx\tthe ADX file to work with\n");
    } else {
        printf("known keys:\n\n");
        for (i=0;i<KEY_COUNT;i++) {
            printf("Key id %d\n",i);
            printf("\tName:\t%s\n",keys[i].name);
            printf("\tStart:\t%04x\n",keys[i].start);
            printf("\tMult:\t%04x\n",keys[i].mult);
            printf("\tAdd:\t%04x\n",keys[i].add);
            if (keys[i].type9) {
                printf("\tType:\t9\n");
            } else {
                printf("\tType:\t8\n");
            }
        }
    }
    printf("\n");
}

int main(int argc, char ** argv) {
    FILE * infile = NULL, * outfile = NULL;
    int off,i;
    int xor;
    int keyid=0;
    int xorstart;
    int xormult;
    int xoradd;
    int keyidset=0;
    int addset=0,multset=0,startset=0;
    int encrypt=0;
    int brute=0;
    int bruteframe=0,bruteframecount=-1;
    int * scales = NULL;
    int scalecount = 0;
    unsigned char buf[18];
    int startoff, endoff;
    int mask=0x7fff;
    int different=0;
    int diffwhich;
    char * infilename = NULL, * outfilename = NULL;

    if (argc<2) {
        usage(argv[0],0);
        return 1;
    }

    /* parse command line */
    for (i=1;i<argc;i++) {
        if (argv[i][0]=='-') {
            if (strlen(argv[i])!=2) {printf("invalid option %s\n",argv[i]); return 1;}
            switch (argv[i][1]) {
                case 'k':
                    if (keyidset) {printf("duplicate -k\n"); return 1;}
                    if (addset || multset || startset) {printf("use only -k or all of -s, -m, and -a\n"); return 1;}
                    if (i+1>=argc) {printf("-k needs a key id\n"); return 1;}
                    if (argv[i+1][0]=='?') {usage(argv[0],1); return 1;}
                    if (sscanf(argv[i+1],"%d",&keyid)!=1 || keyid<0 || keyid>=KEY_COUNT) {printf("bad key id given to -k\n"); return 1;}
                    keyidset=1;
                    i++;
                    break;
                case 's':
                case 'm':
                case 'a':
                    if (keyid) {printf("use only -k or all of -s, -m, and -a\n"); return 1;}
                    {
                        int * setted;
                        int * val;
                        if (argv[i][1]=='s') {setted=&startset; val=&xorstart;}
                        if (argv[i][1]=='m') {setted=&multset; val=&xormult;}
                        if (argv[i][1]=='a') {setted=&addset; val=&xoradd;}
                        if (*setted) {printf("duplicate %s\n",argv[i]); return 1;}
                        if (i+1>=argc || sscanf(argv[i+1],"%x",val)!=1) {printf("%s needs a hex value\n",argv[i]); return 1;}
                        *setted=1;
                        i++;
                    }
                    break;
                case 'e':
                    if (encrypt) {printf("duplicate -e\n"); return 1;}
                    encrypt=1;
                    break;
                case 'b':
                    if (brute) {printf("duplicate -b\n"); return 1;}
                    brute=1;
                    break;
                case 'f':
                    if (i+1>=argc || sscanf(argv[i+1],"%x",&bruteframe)!=1) {printf("-f needs a frame number\n"); return 1;}
                    i++;
                    break;
                case 'n':
                    if (i+1>=argc || sscanf(argv[i+1],"%x",&bruteframecount)!=1) {printf("-n needs a frame count\n"); return 1;}
                    i++;
                    break;
                case 'd':
                    if (different) {printf("duplicate -d\n"); return 1;}
                    if (i+1>=argc || sscanf(argv[i+1],"%x",&diffwhich)!=1 || (diffwhich!=0 && diffwhich!=1)) {printf("-d needs a channel number, 0 or 1\n"); return 1;}
                    different=1;
                    i++;
                    break;
                case 'o':
                    if (outfilename) {printf("duplicate -o\n"); return 1;}
                    if (i+1>=argc) {printf("-o needs a file name\n"); return 1;}
                    outfilename=argv[i+1];
                    i++;
                    break;
                case 'M':
                    if (mask==0x1fff) {printf("duplicate -M\n"); return 1;}
                    mask=0x1fff;
                    break;
                default:
                    printf("unknown switch %s\n",argv[i]);
                    return 1;
            } /* end of command line switch switch */
        } else { /* if argument doesn't start with - */
            if (infilename) {printf("only specify one input file\n"); return 1;}
            infilename=argv[i];
        }
    }

    /* check for conflicts */
    if (brute && (encrypt || keyid || outfilename)) {printf("-b only goes with -f, -n, -s, -m, and -a\n"); return 1;}
    if ((!keyidset) && !brute && (!startset || !multset || !addset)) {printf("key must be specified, either with -k or all of -s, -m, and -a\n"); return 1;}
    if (!infilename) {printf("must specify an input file\n"); return 1;}
    if (outfilename && !strcmp(infilename,outfilename)) {printf("using the same file for input and output in this manner will not work\n"); return 1;}

    /* fetch the particular key by id */
    if (keyidset) {
        xorstart=keys[keyid].start;
        xormult=keys[keyid].mult;
        xoradd=keys[keyid].add;
        if (keys[keyid].type9) {
            mask=0x1fff;
        }
    }

    /* open files */
    if (!outfilename) {
        if (brute) infile=fopen(infilename,"rb");
        else infile=fopen(infilename,"r+b");
        outfile=infile;
    } else {
        infile=fopen(infilename,"rb");
        outfile=fopen(outfilename,"wb");
        if (!outfile) {printf("failed to open output file %s\n",outfilename); return 1;}
    }
    if (!infile) {printf("failed to open input file %s\n",infilename); return 1;}

    /* copy input file to output file */
    if (outfilename) {
#define DUMPBUFSIZE 512
        char dumpbuf[DUMPBUFSIZE];
        int dumped;
        while (dumped=fread(dumpbuf,1,DUMPBUFSIZE,infile))
            if (fwrite(dumpbuf,1,dumped,outfile)!=dumped) {printf("error writing output file %s\n",outfilename); return 1;}
    }

    /* read header */
    fseek(infile,0,SEEK_SET);
    fread(buf,16,1,infile);
    if (buf[0]!=0x80 || buf[1]!=0x00) {
        printf("%s is not ADX\n",infilename);
        return 1;
    }
    if (buf[5]!=18) {
        printf("%s does not have 18-byte frames, how odd... FAIL\n",infilename);
        return 1;
    }

    startoff=read16(buf+2)+4;
    endoff=(read32(buf+12)+31)/32*18*buf[7]+startoff;

    /* get version, encryption flag */
    fread(buf,4,1,infile);
    if (!encrypt) {
        if (buf[3]!=8 && buf[3]!=9) {printf("%s doesn't seem to be encrypted\n",infilename); return 1;}
        buf[3]=0;
    } else {
        if (buf[3]==8) {printf("%s seems to be already encrypted\n",infilename); return 1;}
        buf[3]=8;
    }

    if (!brute) {
        /* clear/set encryption flag */
        fseek(outfile,0x10,SEEK_SET);
        fwrite(buf,4,1,outfile);
    }

    if (!brute) xor=xorstart;
    if (brute) {
        int framecount=endoff-startoff/18;
        if (framecount<bruteframecount || bruteframecount<0) bruteframecount=framecount;
        scales = malloc(sizeof(int) * bruteframecount);
        if (!scales) {printf("out of memory, try a smaller -n\n"); return 1;}
        scalecount=0;
    }
    for (off=startoff,i=0;off<endoff;off+=18,i++) {
        fseek(infile,off,SEEK_SET);
        fread(buf,18,1,infile);

        if (different && (i&1)!=diffwhich) continue;
        if (!brute) {
            write16((read16(buf)^xor)&mask,buf);
            fseek(outfile,off,SEEK_SET);
            fwrite(buf,18,1,outfile);

            xor=(xor*xormult+xoradd)&0x7fff;
        } else {
            if (i>=bruteframe && scalecount<bruteframecount)
                scales[scalecount++]=read16(buf);
        }
    }

    if (brute) {
        guess_xor(scales,scalecount,startset,xorstart,addset,xoradd,multset,xormult,mask);
    }
}

/* Brute force estimation. I assume that the correct value will have the lowest total scales (as they will all be within reasonable levels). */
void guess_xor(int * scales, int scalecount, int startset, int startguess, int addset, int addguess, int multset, int multguess, int mask) {
    int startoff, start_firstguess;
    int minestimate=(mask+1)*scalecount;

    printf("max\t%d\n",minestimate);
    if (minestimate<0) {printf("minestimate overflow, choose a smaller -n\n"); return;}
    if (startset) start_firstguess=startguess;
    else start_firstguess=scales[0];

    if(addset && multset) {
        minestimate=guess_from_low(scales,scalecount,start_firstguess,minestimate,mask,addguess,multguess);
        for (startoff=1;startoff<0x1000;startoff++) {
            minestimate=guess_from_low(scales,scalecount,start_firstguess+startoff,minestimate,mask,addguess,multguess);
            minestimate=guess_from_low(scales,scalecount,start_firstguess-startoff,minestimate,mask,addguess,multguess);
        }
    } else if (multset) {
        minestimate=guess_from_mult(scales,scalecount,start_firstguess,minestimate,mask,multguess);
        for (startoff=1;startoff<0x1000;startoff++) {
            minestimate=guess_from_mult(scales,scalecount,start_firstguess+startoff,minestimate,mask,multguess);
            minestimate=guess_from_mult(scales,scalecount,start_firstguess-startoff,minestimate,mask,multguess);
        }
    } else {
        minestimate=guess_from_start(scales,scalecount,start_firstguess,minestimate,mask);
        for (startoff=1;startoff<0x1000;startoff++) {
            minestimate=guess_from_start(scales,scalecount,start_firstguess+startoff,minestimate,mask);
            minestimate=guess_from_start(scales,scalecount,start_firstguess-startoff,minestimate,mask);
        }
    }
}

void printguess(int start, int mult, int add, int scaletotal, int scalecount) {
    printf("-s %04x -m %04x -a %04x\tscaletotal\t%d\t%d\n",start,mult,add,scaletotal,scaletotal/scalecount);
}
int guess_from_start(int * scales, int scalecount, int start, int minestimate, int mask) {
    int add,mult,i;
    for (add=0;add<mask+1;add++) {
        for (mult=0;mult<mask+1;mult++) {
            int xor=start;
            int total=0;
            for (i=0;i<scalecount;i++) {
                total+=(scales[i]^xor)&mask;
                xor=(xor*mult+add)&0x7fff;
            }
            if (total<=minestimate) {
                minestimate=total;
                printguess(start,mult,add,total,scalecount);
            }

        }
    }
    return minestimate;
}

int guess_from_low(int * scales, int scalecount, int start, int minestimate, int mask, int addguess, int multguess) {
    int add,mult,i;
    for (add=addguess;add<mask+1;add+=0x100) {
        for (mult=multguess;mult<mask+1;mult+=0x100) {
            int xor=start;
            int total=0;
            for (i=0;i<scalecount;i++) {
                total+=(scales[i]^xor)&mask;
                xor=(xor*mult+add)&0x7fff;
            }
            if (total<=minestimate) {
                minestimate=total;
                printguess(start,mult,add,total,scalecount);
            }

        }
    }
    return minestimate;
}


int guess_from_mult(int * scales, int scalecount, int start, int minestimate, int mask, int multguess) {
    int add,mult,i;
    for (add=0;add<mask+1;add++) {
        mult=multguess;
        {
            int xor=start;
            int total=0;
            for (i=0;i<scalecount;i++) {
                total+=(scales[i]^xor)&mask;
                xor=(xor*mult+add)&0x7fff;
            }
            if (total<=minestimate) {
                minestimate=total;
                printguess(start,mult,add,total,scalecount);
            }
        }

    }
    return minestimate;
}
