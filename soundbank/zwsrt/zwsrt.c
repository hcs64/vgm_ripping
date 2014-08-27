#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void try(const char * const what, int result) {
    if (!result) return;
    fprintf(stderr,"error: %s failed\n",what);
    exit(1);
}

unsigned int read32(unsigned char * buf) {
    return (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
}
unsigned int read16(unsigned char * buf) {
    return (buf[0]<<8)|(buf[1]);
}

void hexprint(const unsigned char * bytes, int len) {
    int i;
    for (i=0;i<len;i++) {
        if (i>0 && i%16 == 0) printf("\n");
        printf("%02x ",bytes[i]);
    }
    printf("\n");
}

#define BUFSIZE 0x2000
unsigned char buf[BUFSIZE];

#if 0
int main(int argc, char ** argv) {
    int block_RND;
    int block_SSD;
    int block_SRT;
    int block_DSP;
    int stream_count;
    int file_length;
    int i;

    FILE * infile;

    if (argc!=2) {printf("usage: %s blah.srt\n",argv[0]); return 1;}

    try("open input",(infile=fopen(argv[1],"rb"))==NULL);

    printf("%s...\n",argv[1]);

    try("seek to file end",fseek(infile,0,SEEK_END));
    try("get file size",(file_length=ftell(infile))==-1);
    try("get back to file start",fseek(infile,0,SEEK_SET));

    try("read header",fread(buf,1,16,infile)!=16);

    block_RND=read32(buf);
    block_SSD=read32(buf+4);
    block_SRT=read32(buf+8);
    block_DSP=read32(buf+12);

    try("seek to RND block",fseek(infile,block_RND,SEEK_SET));
    try("read RND block header ",fread(buf,1,16,infile)!=16);
    try("check RND block header",memcmp("RND\0\0\0\0\0\0\0\0\x10\0\0\0\0",buf,16));

    try("seek to SSD block",fseek(infile,block_SSD,SEEK_SET));
    try("read SSD block header",fread(buf,1,16,infile)!=16);
    try("check SSD block header",memcmp("SSD\0",buf,4)||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));
    stream_count = read32(buf+4);

    printf("%d streams\n",stream_count);

    /* look at SSD entries */
    for (i=0;i<stream_count;i++) {
        try("seek to SSD entry index",fseek(infile,block_SSD+0x10+i*4,SEEK_SET));
        try("read SSD entry index",fread(buf,1,4,infile)!=4);
        try("seek to SSD entry",fseek(infile,block_SSD+0x10+read32(buf),SEEK_SET));
        try("read SSD entry",fread(buf,1,16,infile)!=16);

        if (read16(buf)!=i || memcmp("\0\0\0\0",buf+2,4) || (buf[6]!='\x40' && buf[6]!='\x7f') || memcmp("\x40\x7f\0\x3f\0\x3f\0\0\0",buf+7,9)) {
            printf("%i has unusual SSD entry\n",i);
            hexprint(buf,16);
        }

        if (buf[6]!='\x40')
            printf("%i flag: %02x\n",i,buf[6]);
    }

    try("seek to SRT block",fseek(infile,block_SRT,SEEK_SET));
    try("read SRT block header",fread(buf,1,16,infile)!=16);
    /* always seems to have 0x80 entries */
    try("check SRT block header",memcmp("SRT\0\0\0\0\x80\0\0\0\x10\0\0\0\0",buf,16));

    /* look at SRT entries */
    for (i=0;i<0x80;i++) {
        int flag00,flag09,flag0f;
        try("seek to SRT entry index",fseek(infile,block_SRT+0x10+i*4,SEEK_SET));
        try("read SRT entry index",fread(buf,1,4,infile)!=4);
        try("seek to SRT entry",fseek(infile,block_SRT+0x10+read32(buf),SEEK_SET));
        try("read SRT entry",fread(buf,1,0x30,infile)!=0x30);

        if (i>=stream_count) continue;

        if (memcmp("\xff\xff\x64\x1",buf+1,4) || buf[5]!=i || memcmp("\x64\0\x40",buf+6,3) || memcmp("\xff\x7f\xff\xff\0",buf+10,5)) {
            printf("%i had unusual SRT start\n",i);
            hexprint(buf,15);
        }

        flag00=buf[0];
        flag09=buf[9];
        flag0f=buf[15];
        if (i<stream_count) {
            printf("%i flags: [0]=%#02x [9]=%#02x [15]=%#02x\n",i,flag00,flag09,flag0f);
            if (buf[9]!=1) printf("flag9 should have been 0x01\n");
            if (buf[15]==0x7f) printf("flag15 shouldn't have been 0x7f\n");
        } else if (flag0f!=0x7f) {
            printf("%i (nonexistant) flags: [0]=%#02x [9]=%#02x [15]=%#02x\n",i,flag00,flag09,flag0f);
        }

        if (read32(buf+0x10)!=0 || read32(buf+0x14)!=read32(buf+0x18) ||
                read32(buf+0x18)!=read32(buf+0x1c) || read32(buf+0x1c)!=0xffffffff) {
            printf("%i had unusual SRT end\n",i);
            hexprint(buf+0x10,0x20);
        }
    }

    try("seek to DSP block",fseek(infile,block_DSP,SEEK_SET));
    try("read DSP block header",fread(buf,1,16,infile)!=16);
    try("check DSP block header",memcmp("DSP\0",buf,4) || read32(buf+4)!=stream_count ||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));

    /* look at DSP entries */
    {
        int lastindex=-1;
        for (i=0;i<stream_count;i++) {
            int offset;
            try("seek to DSP entry index",fseek(infile,block_DSP+0x10+i*4,SEEK_SET));
            try("read DSP entry index",fread(buf,1,4,infile)!=4);

            offset=read32(buf);

            try("seek to DSP entry",fseek(infile,block_DSP+0x10+offset,SEEK_SET));
            try("read DSP entry",fread(buf,1,144,infile)!=144);

            if (i==0) lastindex=offset;
            else {
                if (offset-lastindex != 144) printf("BAD SIZE\n");
                /*printf("%d size: %d\n",i-1,offset-lastindex);*/
                lastindex=offset;
            }
            printf("%d:\n",i);
            hexprint(buf,144);
            printf("%3d:\t%d\n",i,read32(buf+0x58));
        }
        if (file_length-(lastindex+block_DSP+0x10) != 144) printf("BAD SIZE\n");
        /*printf("%d size: %d\n",i-1,file_length-(lastindex+block_DSP+0x10));*/
    }

    return 0;
}
#endif

int main(int argc, char ** argv) {
    int block_RND;
    int block_SSD;
    int block_SRT;
    int block_DSP;
    int stream_count;
    int srt_entry_count;
    int file_length;
    int i;

    FILE * infile;

    if (argc!=5 && argc!=2) {printf("usage: %s blah.srt [2 blah02.ssd out.zwdsp]\n",argv[0]); return 1;}

    try("open input",(infile=fopen(argv[1],"rb"))==NULL);


    try("seek to file end",fseek(infile,0,SEEK_END));
    try("get file size",(file_length=ftell(infile))==-1);
    try("get back to file start",fseek(infile,0,SEEK_SET));

    try("read header",fread(buf,1,16,infile)!=16);

    block_RND=read32(buf);
    block_SSD=read32(buf+4);
    block_SRT=read32(buf+8);
    block_DSP=read32(buf+12);

    if (argc==2) { /* examine mode */
        printf("%s...\n",argv[1]);

        try("seek to RND block",fseek(infile,block_RND,SEEK_SET));
        try("read RND block header ",fread(buf,1,16,infile)!=16);
        try("check RND block header",memcmp("RND\0\0\0\0\0\0\0\0\x10\0\0\0\0",buf,16));

        try("seek to SSD block",fseek(infile,block_SSD,SEEK_SET));
        try("read SSD block header",fread(buf,1,16,infile)!=16);
        try("check SSD block header",memcmp("SSD\0",buf,4)||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));
        stream_count = read32(buf+4);

        printf("%d streams\n",stream_count);

        /* look at SSD entries */
        for (i=0;i<stream_count;i++) {
            try("seek to SSD entry index",fseek(infile,block_SSD+0x10+i*4,SEEK_SET));
            try("read SSD entry index",fread(buf,1,4,infile)!=4);
            try("seek to SSD entry",fseek(infile,block_SSD+0x10+read32(buf),SEEK_SET));
            try("read SSD entry",fread(buf,1,16,infile)!=16);

            /*
               if (read16(buf)!=i || memcmp("\0\0\0\0",buf+2,4) || (buf[6]!='\x40' && buf[6]!='\x7f') || memcmp("\x40\x7f\0\x3f\0\x3f\0\0\0",buf+7,9)) {
               printf("%i has unusual SSD entry\n",i);
               hexprint(buf,16);
               }

               if (buf[6]!='\x40')
               printf("%i flag: %02x\n",i,buf[6]);
               */
        }

        try("seek to SRT block",fseek(infile,block_SRT,SEEK_SET));
        try("read SRT block header",fread(buf,1,16,infile)!=16);
        /* always seems to have 0x80 entries */
        //try("check SRT block header",memcmp("SRT\0\0\0\0\x80\0\0\0\x10\0\0\0\0",buf,16));
        try("check SRT block header",memcmp("SRT\0",buf,4) || memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));
        srt_entry_count = read32(buf+4);

        /* look at SRT entries */
        for (i=0;i<srt_entry_count;i++) {
            int flag00,flag09,flag0f;
            try("seek to SRT entry index",fseek(infile,block_SRT+0x10+i*4,SEEK_SET));
            try("read SRT entry index",fread(buf,1,4,infile)!=4);
            try("seek to SRT entry",fseek(infile,block_SRT+0x10+read32(buf),SEEK_SET));
            try("read SRT entry",fread(buf,1,0x30,infile)!=0x30);

            if (i>=stream_count) continue;

            /*
               if (memcmp("\xff\xff\x64\x1",buf+1,4) || buf[5]!=i || memcmp("\x64\0\x40",buf+6,3) || memcmp("\xff\x7f\xff\xff\0",buf+10,5)) {
               printf("%i had unusual SRT start\n",i);
               hexprint(buf,15);
               }

               flag00=buf[0];
               flag09=buf[9];
               flag0f=buf[15];
               if (i<stream_count) {
               printf("%i flags: [0]=%#02x [9]=%#02x [15]=%#02x\n",i,flag00,flag09,flag0f);
               if (buf[9]!=1) printf("flag9 should have been 0x01\n");
               if (buf[15]==0x7f) printf("flag15 shouldn't have been 0x7f\n");
               } else if (flag0f!=0x7f) {
               printf("%i (nonexistant) flags: [0]=%#02x [9]=%#02x [15]=%#02x\n",i,flag00,flag09,flag0f);
               }

               if (read32(buf+0x10)!=0 || read32(buf+0x14)!=read32(buf+0x18) ||
               read32(buf+0x18)!=read32(buf+0x1c) || read32(buf+0x1c)!=0xffffffff) {
               printf("%i had unusual SRT end\n",i);
               hexprint(buf+0x10,0x20);
               }
               */
        }

        try("seek to DSP block",fseek(infile,block_DSP,SEEK_SET));
        try("read DSP block header",fread(buf,1,16,infile)!=16);
        try("check DSP block header",memcmp("DSP\0",buf,4) || read32(buf+4)!=stream_count ||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));

        /* look at DSP entries */
        {
            int lastindex=-1;
            for (i=0;i<stream_count;i++) {
                int offset;
                try("seek to DSP entry index",fseek(infile,block_DSP+0x10+i*4,SEEK_SET));
                try("read DSP entry index",fread(buf,1,4,infile)!=4);

                offset=read32(buf);

                try("seek to DSP entry",fseek(infile,block_DSP+0x10+offset,SEEK_SET));
                try("read DSP entry",fread(buf,1,144,infile)!=144);

                if (i==0) lastindex=offset;
                else {
                    if (offset-lastindex != 144) printf("BAD SIZE\n");
                    /*printf("%d size: %d\n",i-1,offset-lastindex);*/
                    lastindex=offset;
                }
                /*printf("%d:\n",i);
                hexprint(buf,144);*/
                printf("%3d:\t%d\n",i,read32(buf+0x58));
            }
            if (file_length-(lastindex+block_DSP+0x10) != 144) printf("BAD SIZE\n");
            /*printf("%d size: %d\n",i-1,file_length-(lastindex+block_DSP+0x10));*/
        }
        fclose(infile);

        return 0;
    } else if (argc==5) { /* dump mode */
        FILE * outfile;
        FILE * infile_data;
        int todump;
        int ssd_size;


        try("open input ssd file",(infile_data=fopen(argv[3],"rb"))==NULL);
        try("seek to ssd file end",fseek(infile_data,0,SEEK_END));
        try("get ssd file size",(ssd_size=ftell(infile_data))==-1);
        try("seek back to ssd file start",fseek(infile_data,0,SEEK_SET));

        try("seek to SSD block",fseek(infile,block_SSD,SEEK_SET));
        try("read SSD block header",fread(buf,1,16,infile)!=16);
        try("check SSD block header",memcmp("SSD\0",buf,4)||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));
        try("open output file",(outfile=fopen(argv[4],"wb"))==NULL);
        try("parse stream # to dump",sscanf(argv[2],"%d",&todump)!=1);

        printf("%s[%d]+%s->%s...\n",argv[1],todump,argv[3],argv[4]);

        stream_count = read32(buf+4);

        if (todump<0 || todump>=stream_count) {
            printf("bad stream number, there are %d stream headers in this .srt\n",stream_count);
            exit(1);
        }

        try("seek to DSP entry index",fseek(infile,block_DSP+0x10+todump*4,SEEK_SET));
        try("read DSP entry index",fread(buf,1,4,infile)!=4);
        try("seek to DSP entry",fseek(infile,block_DSP+0x10+read32(buf),SEEK_SET));
        try("read DSP entry",fread(buf,1,144,infile)!=144);

        try("write DSP entry",fwrite(buf,1,144,outfile)!=144);

        while (ssd_size>0) {
            int amt=ssd_size>BUFSIZE?BUFSIZE:ssd_size;

            /*printf("%d left (at %d), reading %d\n",ssd_size,ftell(infile_data),amt);*/
            try("read ssd",fread(buf,1,amt,infile_data)!=amt);
            try("write output",fwrite(buf,1,amt,outfile)!=amt);
            ssd_size-=amt;
        }
        fclose(outfile); outfile=NULL;
        fclose(infile_data); infile_data=NULL;
        fclose(infile);

        return 0;
    }

    return 1;
}
