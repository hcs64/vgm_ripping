// gcfsysd 0.1
// unpack .fsys files as found in Pokemon XD Glade of Darkness and Pokemon Colloseum
// by hcs

// 0.1: better support for fsys containing compressed files, better name detection

#include <stdio.h>
#include <string.h>

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

int main(int argc, char ** argv) {
    FILE * infile, * outfile;
    char buf[1025];
    int fsyssize,headpos,datapos;
    int fheadertab,fheader,fname,fname2,fdata;
    int fcount,fsize,fnum; // total number of files, file size, cur file #
    int i;

    printf("gcfsysd 0.1\n\n");

    if (argc!=2) {printf("argument is .fsys to unpack\n"); return 1;}
    if (!(infile=fopen(argv[1],"rb"))) {printf("error opening %s\n",argv[1]); return 1;}

    // ******** read header 1

    // 0x00-0x07: signature (?)

    fread(buf,1,8,infile);
    if (memcmp(buf,"FSYS",4)) {printf("file format not recognized\n"); return 1;}

    // 0x08-0x0B: identifier, specified in gcfsys.toc (not used)
    // 0x0C-0x0F: file count (?)

    fread(buf,1,8,infile);

    // I *think* this may be the file count...
    fcount = get32bit(buf+4);

    // 0x10-0x17: 0x800000xx00000003 (xx=00 or 01)
    fseek(infile,8,SEEK_CUR);

    // 0x18-0x1B: header 2 start offset
    // 0x1C-0x1F: data start offset
    // 0x20-0x23: total file size
    // none of these really need to be read for extraction...

    fread(buf,1,12,infile);
    headpos = get32bit(buf);
    datapos = get32bit(buf+4); // unneeded ('cept for info purposes)
    fsyssize = get32bit(buf+8); // unneeded " "

    printf("Header info:\n\tfile count:\t%d\n\theader start:\t%08x\n\tdata start:\t%08x\n\tfile size:\t%08x (%d)\n\n",fcount,headpos,datapos,fsyssize,fsyssize);

    // 0s used as filler between end of header 1 and start of header 1

    // ******* read header 2

    // header2 0x00-0x03: file header pointer table offset
    // header2 0x04-0x07: file name table offset
    // header2 0x08-0x0b: data start offset (dupe)

    fseek(infile,headpos,SEEK_SET);
    fread(buf,1,4,infile);
    fheadertab = get32bit(buf);

    // ******** extract files

    for (fnum=0;fnum<fcount;fnum++) {
        // get file header offset
        fseek(infile,fheadertab,SEEK_SET);
        fheadertab+=4;
        fread(buf,1,4,infile);

        fheader = get32bit(buf);

        // go to file header
		fseek(infile,fheader+4,SEEK_SET);

        // file header 0x00-0x01: unknown
        // file header 0x02: unknown
        // file header 0x03: 0

        // file header 0x04-0x07: file data start
        // file header 0x08-0x0B: file data size (uncompressed)
        
        fread(buf,1,0x20,infile);
        fdata = get32bit(buf);
        //fsize = get32bit(buf+4);

        // file header 0x0C: either 0 or 0x80 (flag for compressed data?)
		// file header 0x0D-0x13: 0s
        // file header 0x14-0x17: file data size (compressed)

		fsize = get32bit(buf+0x10);

        // file header 0x18-0x1B: 0s
        // file header 0x1C-0x1F: pointer to null term file name (can be 0)
        // file header 0x20-0x23: unknown
        // file header 0x24-0x27: pointer to null term other file name (always, though might be
		//						  useless, like "(null)")
        
        fseek(infile,fheader+0x1C,SEEK_SET);
        fread(buf,1,12,infile);
        
        if ((fname=get32bit(buf)) || (fname=get32bit(buf+8))) {
            fseek(infile,fname,SEEK_SET);
            fread(buf,1,512,infile);
            printf("\tFile %02d info:\n\t\tname: \"%s\"\n\t\toffset:\t%08x\n\t\tsize:\t%08x (%d)\n\n",fnum,buf,fdata,fsize,fsize);
        }
		if (!fname || !strcmp(buf,"(null)")) {
		    char *namebase,*tp,t;

            // generate a name if none is given
            namebase=strrchr(argv[1],'\\');
            if (!namebase) namebase=argv[1];
            else namebase++;
            tp=strrchr(argv[1],'.');
            if (!tp) tp=argv[1]+strlen(argv[1]);
            t=tp[0];
            tp[0]='\0';
            sprintf(buf,"%s.%03d",namebase,fnum);
            tp[0]=t;

            printf("\tFile %02d info:\n\t\tmade-up name: \"%s\"\n\t\toffset:\t%08x\n\t\tsize:\t%08x (%d)\n\n",fnum,buf,fdata,fsize,fsize);
        }

        if (!(outfile = fopen(buf,"wb"))) {printf("error opening %s\n",buf); return 1;}

        fseek(infile,fdata,SEEK_SET);
        for (i=0;i<fsize;i++) {
            fread(buf,1,1,infile);
            fwrite(buf,1,1,outfile);
        }
        fclose(outfile);
    }
    fclose(infile);
    
    return 0;
}
