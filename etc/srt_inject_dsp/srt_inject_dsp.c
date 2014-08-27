#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void try(const char * const what, int result) {
    if (!result) return;
    int errno_local = errno;

    printf("\n");
    fflush(stdout);
    fprintf(stderr,"error: %s failed\n",what);
    if (errno_local != 0)
    {
        errno = errno_local;
        perror(NULL);
    }
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

enum {DSPSIZE=144};
/* BUFSIZ is at least 256, so a DSPSIZE header will fit */
unsigned char buf[BUFSIZ];
const char padding[0x40];

void write_file(FILE *infile, FILE *outfile, long total_bytes, long padding_bytes)
{
    long bytes_left;
    for ( bytes_left = total_bytes; bytes_left >= BUFSIZ; bytes_left -= BUFSIZ)
    {
        try("read data",fread(buf,1,BUFSIZ,infile)!=BUFSIZ);
        try("write data",fwrite(buf,1,BUFSIZ,outfile)!=BUFSIZ);
    }
    if ( bytes_left > 0 )
    {
        try("read data",fread(buf,1,bytes_left,infile)!=bytes_left);
        try("write data",fwrite(buf,1,bytes_left,outfile)!=bytes_left);
    }
    if ( padding_bytes != 0)
    {
        try("write padding",fwrite(padding,1,padding_bytes,outfile)!=padding_bytes);
    }
}

int main(int argc, char ** argv) {
    long block_RND;
    long block_SSD;
    long block_SRT;
    long block_DSP;
    int stream_count;
    int i;
    int channels = 1;
    int copy_dsp_entry = 0;

    FILE * srtfile = NULL;
    long srt_length;
    FILE * dspfile = NULL;
    long dsp_length;
    FILE * dsp2file = NULL;
    long dsp2_length;

    FILE * ssdfile;
    int tocopy;
    long dspoffset;
    long dsp_data_size;

    FILE * src_srtfile = NULL;
    long src_srt_length;
    int toreplace;

    if (argc!=5 && argc!=6) {
        printf("srt_inject_dsp 0.1\n\n");
        printf("usage: %s blah.srt 2 blah.ssd blah02.dsp [blah02R.dsp]\n",argv[0]);
        printf("    or %s dest.srt 2 -c src.srt 3\n",argv[0]);
        return 1;
    }

    if (argc == 6 && !strcmp(argv[3],"-c"))
    {
        copy_dsp_entry = 1;
    }
    else
    {
        if (argc == 6)
        {
            channels = 2;
        }
        else
        {
            printf("mono is yet unknown\n");
            exit(1);
        }
    }

    try("open target srt",(srtfile=fopen(argv[1],"r+b"))==NULL);
    try("seek to srt file end",fseek(srtfile,0,SEEK_END));
    try("get srt file size",(srt_length=ftell(srtfile))==-1);
    try("get back to srt file start",fseek(srtfile,0,SEEK_SET));

    if (copy_dsp_entry)
    {
        try("open source srt",(src_srtfile=fopen(argv[4],"rb"))==NULL);
        try("seek to src srt file end",fseek(src_srtfile,0,SEEK_END));
        try("get src srt file size",(src_srt_length=ftell(src_srtfile))==-1);
        try("get back to src srt file start",fseek(src_srtfile,0,SEEK_SET));
    }
    else
    {
        try("open dsp file",(dspfile=fopen(argv[4],"rb"))==NULL);
        try("seek to dsp file end",fseek(dspfile,0,SEEK_END));
        try("get dsp file size",(dsp_length=ftell(dspfile))==-1);
        try("seek back to dsp file start",fseek(dspfile,0,SEEK_SET));

        if (channels == 2)
        {
            try("open second dsp file",(dsp2file=fopen(argv[5],"rb"))==NULL);
            try("seek to second dsp file end",fseek(dsp2file,0,SEEK_END));
            try("get second dsp file size",(dsp2_length=ftell(dsp2file))==-1);
            try("seek back to second dsp file start",fseek(dsp2file,0,SEEK_SET));
        }

        try("open ssd",(ssdfile=fopen(argv[3],"wb"))==NULL);
    }

    try("read srt header",fread(buf,1,16,srtfile)!=16);

    block_RND=read32(buf);
    block_SSD=read32(buf+4);
    block_SRT=read32(buf+8);
    block_DSP=read32(buf+12);

    try("parse stream # to replace",sscanf(argv[2],"%d",&toreplace)!=1);

    if (copy_dsp_entry)
    {
        try("parse stream # to copy",sscanf(argv[5],"%d",&tocopy)!=1);
    }

    /* print! */
    if (!copy_dsp_entry)
    {
        printf("%s",argv[4]);
        if (channels == 2)
        {
            printf(",%s",argv[5]);
        }
        printf("->%s[%d]+%s...",argv[1],toreplace,argv[3]);
    }
    else
    {
        printf("%s[%d]->%s[%d]...",argv[4],tocopy,argv[1],toreplace);
    }


    /* NOTE: only stereo here on out */

    /* old check against SSD entry count */
#if 0
    try("seek to SSD block",fseek(srtfile,block_SSD,SEEK_SET));
    try("read SSD block header",fread(buf,1,16,srtfile)!=16);
    try("check SSD block header",memcmp("SSD\0",buf,4)||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));

    stream_count = read32(buf+4);

    if (toreplace<0 || toreplace>=stream_count) {
        printf("bad stream number, there are %d stream headers in this .srt\n",stream_count);
        exit(1);
    }
#endif

    try("seek to DSP block",fseek(srtfile,block_DSP,SEEK_SET));
    try("read DSP block header",fread(buf,1,16,srtfile)!=16);
    try("check DSP block header",memcmp("DSP\0",buf,4)||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));

    /* check stream count */
    stream_count = read32(buf+4);

    if (toreplace<0 || toreplace>=stream_count) {
        printf("bad stream number, there are %d stream headers in this .srt\n",stream_count);
        exit(1);
    }

    try("seek to DSP entry index",fseek(srtfile,block_DSP+0x10+toreplace*4,SEEK_SET));
    try("read DSP entry index",fread(buf,1,4,srtfile)!=4);
    dspoffset = read32(buf);

    /* check DSP size */
    {
        long next_offset = srt_length - (block_DSP+0x10);
        if (toreplace < (stream_count-1))
        {
            try("seek to next DSP entry index",fseek(srtfile,block_DSP+0x10+(toreplace+1)*4,SEEK_SET));
            try("read next DSP entry index",fread(buf,1,4,srtfile)!=4);
            next_offset = read32(buf);
        }

        if (next_offset - dspoffset != DSPSIZE)
        {
            printf("DSP table has unexpected entry size %ld\n",
                    next_offset - dspoffset);
            exit(1);
        }
    }

    /* get header for whatever source */
    if (copy_dsp_entry)
    {
        /* locate and copy entry from source .srt */

        long src_block_DSP;
        int src_stream_count;
        long src_dspoffset;

        try("read source srt header",fread(buf,1,16,src_srtfile)!=16);

        src_block_DSP=read32(buf+12);

        try("seek to src DSP block",fseek(src_srtfile,src_block_DSP,SEEK_SET));
        try("read src DSP block header",fread(buf,1,16,src_srtfile)!=16);
        try("check src DSP block header",memcmp("DSP\0",buf,4)||memcmp("\0\0\0\x10\0\0\0\0",buf+8,8));

        /* check stream count */
        src_stream_count = read32(buf+4);

        if (tocopy<0 || tocopy>=src_stream_count) {
            printf("bad stream number, there are %d stream headers in the source.srt\n",src_stream_count);
            exit(1);
        }

        try("seek to DSP entry index",fseek(src_srtfile,src_block_DSP+0x10+tocopy*4,SEEK_SET));
        try("read DSP entry index",fread(buf,1,4,src_srtfile)!=4);
        src_dspoffset = read32(buf);

        /* check DSP size */
        {
            long next_offset = src_srt_length - (src_block_DSP+0x10);
            if (tocopy < (src_stream_count-1))
            {
                try("seek to next src DSP entry index",fseek(src_srtfile,src_block_DSP+0x10+(tocopy+1)*4,SEEK_SET));
                try("read next src DSP entry index",fread(buf,1,4,src_srtfile)!=4);
                next_offset = read32(buf);
            }

            if (next_offset - src_dspoffset != DSPSIZE)
            {
                printf("src DSP table has unexpected entry size %ld\n",
                        next_offset - src_dspoffset);
                exit(1);
            }
        }

        /* copy DSP entry */
        try("seek to src DSP entry",fseek(src_srtfile,src_block_DSP+0x10+src_dspoffset,SEEK_SET));
        try("read src DSP entry",fread(buf,1,DSPSIZE,src_srtfile)!=DSPSIZE);
    }
    else
    {
        /* build DSP entry */
        char newhead[DSPSIZE] = {0,0,0,0, 0,0,0,1, 0,0,0,0, 1,0,0,0};

        try("read input dsp header",fread(buf,1,0x60,dspfile)!=0x60);

        /* copy sample rate */
        memcpy(&newhead[0x08], &buf[0x08], 4);

        /* FIRST CHANNEL */

        /* copy loop start/end */
        memcpy(&newhead[0x10], &buf[0x10], 8);

        /* copy data size */
        memcpy(&newhead[0x18], &buf[0x04], 4);

        /* copy initial nibble addr */
        memcpy(&newhead[0x1C], &buf[0x18], 4);

        /* copy channel codec setup */
        memcpy(&newhead[0x20], &buf[0x1C], 0x30);

        dsp_data_size = read32(&buf[0x04]);

        try("read 2nd input dsp header",fread(buf,1,0x60,dsp2file)!=0x60);

        /* SECOND CHANNEL */

        /* check sample rate */
        try("check 2nd channel srate",memcmp(&newhead[0x08], &buf[0x08], 4));

        /* copy loop start/end */
        memcpy(&newhead[0x50], &buf[0x10], 8);

        /* copy data size */
        memcpy(&newhead[0x58], &buf[0x04], 4);

        /* copy initial nibble addr */
        memcpy(&newhead[0x5C], &buf[0x18], 4);

        /* copy codec setup */
        memcpy(&newhead[0x60], &buf[0x1C], 0x30);

        try("check for matching sizes", dsp_data_size != read32(&buf[0x04]));

        memcpy(buf,newhead,DSPSIZE);
    }

    /* replace DSP entry! */
    try("seek to dest DSP entry",fseek(srtfile,block_DSP+0x10+dspoffset,SEEK_SET));
    try("replace DSP entry",fwrite(buf,1,DSPSIZE,srtfile)!=DSPSIZE);

    /* write data */
    /* This is not supported for -c because it is assumed that the
       user can copy files on his own. */
    if (!copy_dsp_entry)
    {
        /* round nibble count up to 64 bytes */
        const long total_bytes = (dsp_data_size + 0x7F) / 0x80 * 0x40;
        const long padding_bytes = total_bytes - ((dsp_data_size + 1) / 2);
        const long total_real_bytes = total_bytes - padding_bytes;

        write_file(dspfile,ssdfile,total_real_bytes,padding_bytes);
        write_file(dsp2file,ssdfile,total_real_bytes,padding_bytes);
    }

    /* cleanup */
    try("close dest srt",EOF==fclose(srtfile));
    if (copy_dsp_entry)
    {
        fclose(src_srtfile);
    }
    else
    {
        fclose(dspfile);
        if (dsp2file)
        {
            fclose(dsp2file);
        }
        try("close ssd",EOF==fclose(ssdfile));
    }

    printf(" done!\n");

    return 0;
}
