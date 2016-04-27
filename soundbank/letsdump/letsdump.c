#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "streamfile.h"
#include "util.h"

int dump(STREAMFILE *infile, FILE *outfile, off_t offset, off_t size);

int main(int argc, char ** argv) {
    STREAMFILE * infile;
    FILE *outfile;
    off_t SYMB_off=0, INFO_off=0, FILE_off=0;

    printf("dump FILE chunk of brsar\n");

    if (argc!=3) {fprintf(stderr,"usage: %s infile.brsar output\n",argv[0]); return 1;}

    infile = open_streamfile(argv[1]);

    if (!infile) {fprintf(stderr,"error opening %s for input\n",argv[1]); return 1;}

    outfile = fopen(argv[2],"wb");

    if (!outfile) {fprintf(stderr,"error opening %s for output\n",argv[2]); return 1;}

    {
        char buf[8];
        if (read_streamfile(buf,0,8,infile)!=8) {fprintf(stderr,"error reading\n"); return 1;}

        if (memcmp(buf,"RSAR\xfe\xff",6)) {fprintf(stderr,"not any .brsar I've ever seen\n"); return 1;}
    }

    {
        size_t filesize,sizeinheader;

        filesize = get_streamfile_size(infile);
        sizeinheader = read_32bitBE(8,infile);
        printf("Ok, so I'm reading %s.\nFilesize: %#x ",argv[1],filesize);
        if (filesize==sizeinheader) printf("(header agrees)\n");
        else printf("(header says %#x)\n",sizeinheader);
        printf("Version: %d.%d\n",read_8bit(6,infile),read_8bit(7,infile));
    }

    {
        int chunk_count;
        int header_size;
        int chunk_idx;

        header_size = read_16bitBE(12,infile);
        chunk_count = read_16bitBE(14,infile);
        printf("Header size: %#x\nChunk count: %d\n",header_size,chunk_count);

        for (chunk_idx=0;chunk_idx<chunk_count;chunk_idx++) {
            char chunk_name[5]={0,0,0,0,0};
            size_t chunk_size;
            size_t chunklist_size;
            int chunk_addr;

            chunk_addr=read_32bitBE(0x10+chunk_idx*8,infile);

            if (read_streamfile(chunk_name,chunk_addr,4,infile)!=4) {
                fprintf(stderr,"error reading\n");
                return 1;
            }
            chunk_size = read_32bitBE(chunk_addr+4,infile);
            chunklist_size = read_32bitBE(0x10+chunk_idx*8+4,infile);
            printf("Chunk %d at %#x, called %s, size %#x ",chunk_idx,chunk_addr,chunk_name,chunk_size);
            if (chunk_size==chunklist_size)
                printf("(chunklist agrees)\n");
            else
                printf("(chunklist says %#x)\n",chunklist_size);

            if (chunk_addr<header_size)
                printf("\tWe really weren't expecting to see a chunk inside of the header,\nsomething is probably broken.");

            switch (read_32bitBE(chunk_addr,infile)) {
                case 0x53594D42:    /* SYMB */
                    SYMB_off=chunk_addr+8;
                    break;
                case 0x494E464F:    /* INFO */
                    INFO_off=chunk_addr+8;
                    break;
                case 0x46494C45:    /* FILE */
                    FILE_off=chunk_addr+8;
                    if (dump(infile,outfile,chunk_addr+0x20,chunk_size-0x20) != 0)
                    {
                        fprintf(stderr,"error dumping\n");
                        return 1;
                    }
                    break;
                default:
                    printf("\tdon't know what to do with this chunk, skipping\n");
            }
        }
    }
}

int dump(STREAMFILE *infile, FILE *outfile, off_t offset, off_t size)
{
    unsigned char buf[0x800];
    while (size > 0)
    {
        off_t thistime =
            (size > sizeof(buf)) ?
            sizeof(buf) : size;

        if (read_streamfile(buf, offset, thistime, infile) != thistime)
        {
            return -1;
        }
        if (fwrite(buf, 1, thistime, outfile) != thistime)
        {
            return -1;
        }
        size -= thistime;
        offset += thistime;
    }

    return 0;
}
