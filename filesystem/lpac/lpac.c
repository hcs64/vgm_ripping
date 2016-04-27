#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __MSVCRT__
#define fseeko fseeko64
#define ftello ftello64
#define off_t off64_t
#endif

/*
 * lpac 0.1
 * a simple FPAC extractor (BlazBlue 2), by hcs
 *
 */

uint32_t read32(unsigned char * buf) {
    return (buf[0]) | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
}

void try(const char * what, int result) {
    if (!result) return;
    fprintf(stderr,"\n[%s] failed (%d)\n\n", what, result);
    exit(1);
}

int main(int argc, char ** argv) {
    FILE * infile = NULL;
    unsigned char buf[BUFSIZ];
    off_t header_size;
    off_t file_size;
    off_t name_size;
    off_t alternate_name_buf_size;
    off_t file_entry_size;
    char *outfile_name;
    char *alternate_outfile_name;
    uint32_t file_count;
    uint32_t i;

    if (argc!=2) {
        fprintf(stderr,"lpac 0.1 (FPAC extraction)\nusage: %s blah.pac\n\n",argv[0]);
        return 1;
    }

    fprintf(stderr,"opening %s\n",argv[1]);
    try("open input file",!(infile=fopen(argv[1],"rb")));

    /* first: file size (for various checks) */
    try("seek to end",fseeko(infile,0,SEEK_END));
    file_size=ftello(infile);
    try("seek to start",fseeko(infile,0,SEEK_SET));

    /* must be at least 0x20 bytes (master header) */
    try("size check 1",(file_size<0x20));

    /* check signature */
    try("read header",fread(buf,1,32,infile)!=32);
    try("check header",memcmp(&buf[0],"FPAC",4));

    /* read header values */
    header_size=read32(&buf[0x4]);
    try("size check 2",(file_size!=read32(&buf[0x8])));
    file_count=read32(&buf[0xC]);
    try("header unknown check 1",(1!=read32(&buf[0x10])));
    name_size = read32(&buf[0x14]);
    alternate_name_buf_size = name_size + 0x30 + 1;
    outfile_name = malloc(name_size+1);
    alternate_outfile_name = malloc(alternate_name_buf_size);
    file_entry_size = ((name_size + 0xC) + 16)/16*16;
    try("header padding (?) check 1",(0!=read32(&buf[0x18])));
    try("header padding (?) check 2",(0!=read32(&buf[0x1C])));

    /* check header size */
    try("header size check 1",(header_size>file_size));
    try("header size check 2",(header_size!=0x20+file_entry_size*file_count));

    fprintf(stderr,"%s seems to contain %d files\n",argv[1],file_count);

    for (i=0;i<file_count;i++) {
        uint32_t file_offset;
        uint32_t file_size;
        int j;
        FILE * outfile = NULL;
        printf("file %5d:\t",i);
        /* file entry */
        try("seek to file entry",fseeko(infile,0x20+i*file_entry_size,SEEK_SET));
        try("read file entry",fread(buf,1,file_entry_size,infile)!=file_entry_size);
        
        try("check file index",(i!=read32(&buf[name_size+0])));
        file_offset=header_size + read32(&buf[name_size+4]);
        file_size=read32(&buf[name_size+8]);
        for (j=name_size+0xc;j<file_entry_size;j+=4)
        {
            try("check file padding",(0!=read32(&buf[j])));
        }

        printf("offset %#10x\tsize %#10x\tname ",file_offset,file_size);
        for (j=0;buf[j] && j<name_size;j++) {
            /*printf("%c",buf[j]);*/
            outfile_name[j]=buf[j];
        }
        outfile_name[j]='\0';

        if (j==0) {
            strcpy(outfile_name,"noname");
        }

        /* see if the file already exists, try alternate names */
        {
            int alternate_number = 2;
            FILE * existfile = NULL;
            strcpy(alternate_outfile_name,outfile_name);
            while ((existfile = fopen(alternate_outfile_name,"rb"))) {
                /* opened, uh oh, the file already exists */
                fclose(existfile); existfile = NULL;

                snprintf(alternate_outfile_name,alternate_name_buf_size,"%s_%d",outfile_name,alternate_number++);
            }
            strcpy(outfile_name,alternate_outfile_name);
        }

        printf("%s\n",outfile_name);

        /* dump */
        try("open output file",!(outfile=fopen(outfile_name,"wb")));
        try("seek to file",fseeko(infile,file_offset,SEEK_SET));
        while (file_size) {
            uint32_t todump = (file_size>=BUFSIZ?BUFSIZ:file_size);
            try("read file",fread(buf,1,todump,infile)!=todump);
            try("write file",fwrite(buf,1,todump,outfile)!=todump);
            file_size-=todump;
        }
        try("close outpout file",fclose(outfile));
        outfile=NULL;
    }

    return 0;
}
