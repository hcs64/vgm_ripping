/*
 * guessadx 0.4
 * by hcs
 *
 * find all possible encryption keys for an ADX file
 *
 * Search is restricted to prime multipliers and increments.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

enum {PRIMES_UP_TO = 0x8000};   /* noninclusive */

int read16(unsigned char * buf) {
    return (buf[0]<<8)|buf[1];
}

int read32(unsigned char * buf) {
    return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
}

void usage(const char * binname) {
    fprintf(stderr,"guessadx 0.4\n");
    fprintf(stderr,"usage: %s infile.adx [node_id total_nodes]\n",binname);
    fprintf(stderr,
" For parallel processing, total_nodes is the number of guessadx instances,\n"
" and node_id is an integer from 0 to total_nodes-1 uniquely identifying this\n"
" instance.\n\n");
}

int score(int start, int mult, int add, unsigned short * scales, int scalecount) {
    int xor = start;
    int i;
    int total = 0;
    for (i=0;i<scalecount;i++) {
        if (scales[i] != 0)
            total += (scales[i] ^ xor)&0x7fff;
        xor = xor * mult + add;
    }
    return total;
}

int main(int argc, char ** argv) {
    FILE * infile = NULL;
    int bruteframe=0,bruteframecount=-1;
    unsigned char buf[18];
    int startoff, endoff;
    char * infilename = NULL;
    int primes[PRIMES_UP_TO];
    int isprime[PRIMES_UP_TO];
    int primecount;
    long node_id = 0, total_nodes = 1;

    /* parse command line */

    if (argc != 2) {
        if (argc != 4) {
            usage(argv[0]);
            return 1;
        } else {
            /* parse distributed measures */
            char *endptr;

            if ( 0 == strlen(argv[2])) {
                fprintf(stderr,"invalid node_id\n");
                return 1;
            }
            if ( 0 == strlen(argv[3])) {
                fprintf(stderr,"invalid total_nodes\n");
                return 1;
            }

            errno = 0;
            node_id = strtol(argv[2], &endptr, 10);
            if ( 0 != errno ) {
                perror("node_id");
                return 1;
            }
            if ( argv[2] + strlen(argv[2]) != endptr ) {
                fprintf(stderr, "invalid node_id\n");
                return 1;
            }

            total_nodes = strtol(argv[3], &endptr, 10);
            if ( 0 != errno ) {
                perror("total_nodes");
                return 1;
            }
            if ( argv[3] + strlen(argv[3]) != endptr ) {
                fprintf(stderr, "invalid total_nodes\n");
                return 1;
            }

            if (total_nodes <= 0) {
                fprintf(stderr, "total_nodes out of range\n");
                return 1;
            }

            if (node_id < 0 || node_id >= total_nodes) {
                fprintf(stderr, "node_id out of range\n");
                return 1;
            }
        }
    }

    /* get input file name */
    infilename = argv[1];

    /* open files */
    infile=fopen(infilename,"rb");
    if (!infile) {
        fprintf(stderr,"error opening %s\n",infilename);
        return 1;
    }

    /* read header */
    fseek(infile,0,SEEK_SET);
    if (1 != fread(buf,16,1,infile)) {
        perror("fread of header");
        return 1;
    }
    if (buf[0]!=0x80 || buf[1]!=0x00) {
        fprintf(stderr,"%s is not ADX\n",infilename);
        return 1;
    }
    if (buf[5]!=18) {
        fprintf(stderr,"%s does not have 18-byte frames, how odd... FAIL\n",infilename);
        return 1;
    }

    startoff=read16(buf+2)+4;
    endoff=(read32(buf+12)+31)/32*18*buf[7]+startoff;

    /* get version, encryption flag */
    if (1 != fread(buf,4,1,infile)) {
        perror("fread of version, enc flag");
        return 1;
    }
    if (buf[3]!=8) {
        fprintf(stderr,"%s doesn't seem to be encrypted\n",infilename);
        return 1;
    }

    /* how many scales? */
    {
        int framecount=(endoff-startoff)/18;
        if (framecount<bruteframecount || bruteframecount<0)
            bruteframecount=framecount;
    }

    /* find longest run of nonzero frames */
    {
        int longest=-1,longest_length=-1;
        int i;
        int length=0;
        for (i=0;i<bruteframecount;i++) {
            static const unsigned char zeroes[18]={0};
            unsigned char buf[18];
            fseek(infile,startoff+i*18,SEEK_SET);
            if (1 != fread(buf,18,1,infile))
            {
                perror("fread of scales");
                return 1;
            }
            if (memcmp(zeroes,buf,18)) length++;
            else length=0;
            if (length > longest_length) {
                longest_length=length;
                longest=i-length+1;
                if (longest_length >= 0x8000) break;
            }
        }
        if (longest==-1) {
            fprintf(stderr,"no nonzero frames?\n");
            return 1;
        }
        bruteframecount = longest_length;
        bruteframe = longest;
    }

    /* build list of primes, simple sieve */
    {
        int i,j;
        for (i=2;i<PRIMES_UP_TO;i++) isprime[i] = 1;
        for (i=2;i<PRIMES_UP_TO;i++) {
            for (j=i*i;j<PRIMES_UP_TO;j+=i) isprime[j] = 0;
        }
        for (i=2,primecount=0;i<PRIMES_UP_TO;i++) {
            if (isprime[i]) primes[primecount++] = i;
        }
    }

    {
        /* try to guess key */
#define MAX_FRAMES (INT_MAX/0x8000)
        unsigned short * scales;
        unsigned short * prescales = NULL;
        int scales_to_do;
        static time_t starttime;
        int noback=1;

        /* allocate storage for scales */
        scales_to_do = (bruteframecount > MAX_FRAMES ? MAX_FRAMES : bruteframecount);
        scales = malloc(scales_to_do*sizeof(unsigned short));
        if (!scales) {
            fprintf(stderr,"error allocating memory for scales\n");
            return 0;
        }
        /* prescales are those scales before the first frame we test
         * against, we use these to compute the actual start */
        if (bruteframe > 0) {
            int i;
            /* allocate memory for the prescales */
            prescales = malloc(bruteframe*sizeof(unsigned short));
            if (!prescales) {
                fprintf(stderr,"error allocating memory for prescales\n");
                return 0;
            }
            /* read the prescales */
            for (i=0; i<bruteframe; i++) {
                unsigned char buf[2];
                fseek(infile,startoff+i*18,SEEK_SET);
                if (1 != fread(buf,2,1,infile)) {
                    perror("fread of prescales\n");
                    return 1;
                }
                prescales[i] = read16(buf);
            }
        }

        /* read in the scales */
        {
            int i;
            for (i=0; i < scales_to_do; i++) {
                unsigned char buf[2];
                fseek(infile,startoff+(bruteframe+i)*18,SEEK_SET);
                if (1 != fread(buf,2,1,infile)) {
                    perror("fread of scales");
                    return 1;
                }
                scales[i] = read16(buf);
            }
        }

        /* determine limits of search */
        int scales_per_node = (0x2000+total_nodes-1) / total_nodes;
        int start_scale = scales_per_node * node_id;
        int end_scale = scales_per_node * (node_id+1);
        if (end_scale > 0x2000) {
            end_scale = 0x2000;
        }

        fprintf(stderr,"checking from %x to %x\n",start_scale,end_scale);
        fprintf(stderr,"\n");
        starttime = time(NULL);

        /* do it! */

        /* guess possible low bits for start */
        for (int i=start_scale;i<end_scale;i++) {
            int start = i+(scales[0]&0x6000);
            /* status report */
            if (i>=start_scale+1) {
                char messagebuf[100];
                time_t etime = time(NULL)-starttime;
                time_t donetime = scales_per_node*etime/(i-start_scale)-etime;

                sprintf(messagebuf,"%4x %3d%% %8ld minute%c elapsed %8ld minute%c left (maybe)",
                        i-start_scale,
                        (i-start_scale)*100/scales_per_node,
                        (etime/60),
                        (etime/60)!=1 ? 's' : ' ',
                        (donetime/60),
                        (donetime/60)!=1 ? 's' : ' ');
                if (!noback) {
                    int i;
                    for (i=0;i<strlen(messagebuf);i++)
                        fprintf(stderr,"\b");
                }
                fprintf(stderr,"%s",messagebuf);
                fflush(stderr);
                noback=0;
            }

            /* guess multiplier */
            /* it is assumed that only prime multipliers are used */
            for (int j=0;j<primecount;j++) {
                int mult = primes[j];

                /* guess increment */
                for (int k=0;k<primecount;k++) {
                    int add = primes[k];
                    int xor = start;
                    int s;

                    /* test */
                    for (s=1;s<scales_to_do &&
                            (scales[s]&0x6000)==
                            ((xor = xor * mult + add)&0x6000);s++) {}

                    /* if we tested all values, we have a match */
                    if (s==scales_to_do) {
                        fprintf(stderr,"\n");
                        fflush(stderr);
                        noback=1;

                        /* if our "start" isn't actually on the first frame,
                         * find possible real start values */
                        if (bruteframe>0) {
                            int realstart;
                            int i;
                            for (realstart = 0; realstart < 0x7fff; realstart++) {
                                int xor = realstart;
                                for (i=0;i<bruteframe &&
                                        ((prescales[i]&0x6000)==(xor&0x6000) ||
                                         prescales[i]==0);
                                        i++) {
                                    xor = xor * mult + add;
                                }

                                if (i==bruteframe && (xor&0x7fff)==start) {
                                    printf("-s %4x -m %4x -a %4x (error %d)\n",realstart,mult,add,score(start,mult,add,scales,scales_to_do)+score(realstart,mult,add,prescales,bruteframe));
                                }
                            }
                        } else {
                            printf("-s %4x -m %4x -a %4x (error %d)\n",start,mult,add,score(start,mult,add,scales,scales_to_do));
                        }
                        fflush(stdout);
                    }
                } /* end add for loop */
            } /* end mult for loop */
        } /* end start for loop */
    } /* end key guess section */
    fprintf(stderr,"\n");
    return 0;
}
