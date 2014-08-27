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
 * lafs 0.1
 * a simple AFS extractor, by hcs
 *
 * Written specifically for Naruto Shippuuden Narutimate Hero Accel 2,
 * which seems to throw other extracters.
 *
 * TODO:
 * - method for extracting individual files
 * - list-only mode
 * - date handling, what's the other metadata?
 * - insertion?
 * - use of .afd
 * - recursion (build a whole directory structure out of the internal AFSes)
 */

uint32_t read32(unsigned char * buf) {
    return (buf[0]) | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24);
}

void try(const char * what, int result) {
    if (!result) return;
    fprintf(stderr,"\n[%s] failed (%d)\n\n", what, result);
    exit(1);
}

#ifdef __MINGW32__
#define DIRSEP '\\'
#else
#define DIRSEP '/'
#endif

void make_directory(const char *name)
{
#ifdef __MINGW32__
    _mkdir(name);
#else
    mkdir(name, 0755);
#endif
}

const char *strip_path(const char *path)
{
    const char * c = strrchr(path, DIRSEP);
    if (c)
    {
        // base name starts after last separator
        return c+1;
    }
    else
    {
        // no separators
        return path;
    }
}

FILE * open_file_with_path(const char *base_name, const char *file_name, const char orig_sep, const char *perms)
{
    FILE *f = NULL;
    int file_name_len = 0;;
    char * full_name = NULL;
    int full_name_len = 0;
    int i;

    file_name_len = strlen(file_name);

    full_name = malloc(
        strlen(base_name)+1+
        strlen(file_name)+1);

    if (!full_name)
    {
        return f;
    }

    // start with the base (from name of archive)
    strcpy(full_name, base_name);
    full_name_len = strlen(base_name);

    // walk path, translating directory separators and making dirs
    full_name[full_name_len++] = DIRSEP;
    for (i = 0;  i < file_name_len; i++)
    {
        if (file_name[i] == orig_sep)
        {
            // intermediate directories
            full_name[full_name_len] = '\0';
            make_directory(full_name);
            full_name[full_name_len++] = DIRSEP;
        }
        else
        {
            full_name[full_name_len++] = file_name[i];
        }
    }

    full_name[full_name_len] = '\0';

    f = fopen(full_name, perms);

    free(full_name);

    return f;
}

int main(int argc, char ** argv) {
    FILE * infile = NULL;
    unsigned char buf[2048];
    off_t filename_offset;
    off_t filename_size;
    off_t afs_size;
    uint32_t file_count;
    uint32_t i;

    const char *base_postfix = "_unpacked";
    char *base_name = NULL;

    int no_names;

    if (argc!=2) {
        fprintf(stderr,"lafs 0.3 (AFS extraction)\nusage: %s blah.afs\n\n",argv[0]);
        return 1;
    }

    fprintf(stderr,"opening %s\n",argv[1]);
    try("open input file",!(infile=fopen(argv[1],"rb")));

    base_name = malloc(strlen(argv[1])+strlen(base_postfix)+1);
    strcpy(base_name, strip_path(argv[1]));
    strcat(base_name, base_postfix);
    make_directory(base_name);

    /* first: file size (for various checks) */
    try("seek to end",fseeko(infile,0,SEEK_END));
    afs_size=ftello(infile);
    try("seek to start",fseeko(infile,0,SEEK_SET));

    /* must be at least 8 bytes */
    try("size check 1",(afs_size<8));

    /* check signature */
    try("read header",fread(buf,1,8,infile)!=8);
    try("check header",memcmp(buf,"AFS\0",4));

    /* count files */
    file_count=read32(buf+4);

    /* check for enough space for file entries */
    try("size check 2",afs_size<((file_count+1)*8+8));

    /* locate file name list (there is an entry for this
     * at the end of the files list) */
    try("seek to file name list entry",fseeko(infile,8+file_count*8,SEEK_SET));
    try("read file name list entry",fread(buf,1,8,infile)!=8);
    filename_offset=read32(buf);
    filename_size=read32(buf+4);

    no_names = 1;
    /* check file name list */
    if (filename_offset == 0 || filename_size == 0) {
        /* on DC the name list entry sometimes comes just before the data */
        off_t last_entry;
        try("seek to first data entry",fseeko(infile,8,SEEK_SET));
        try("read first data entry",fread(buf,1,8,infile)!=8);
        last_entry=read32(buf)-8;

        try("seek to file name list entry (alt)",fseeko(infile,last_entry,SEEK_SET));
        try("read file name list entry (alt)",fread(buf,1,8,infile)!=8);

        filename_offset=read32(buf);
        filename_size=read32(buf+4);
            
        if (filename_offset != 0 && filename_size != 0) {
            /* found */
            no_names = 0;
        }
    } else {
        /* plausible name list entry */
        no_names = 0;
    }

    if (!no_names) {
        try("file name list size check",(filename_size!=(file_count*0x30)));
    }

    fprintf(stderr,"%s seems to contain %d files\n",argv[1],file_count);

    for (i=0;i<file_count;i++) {
        uint32_t file_offset;
        uint32_t file_size;
        int j;
        char outfile_name[0x30];
        FILE * outfile = NULL;
        printf("file %5d:\t",i);
        /* file entry */
        try("seek to file entry",fseeko(infile,8+i*8,SEEK_SET));
        try("read file entry",fread(buf,1,8,infile)!=8);
        file_offset=read32(buf);
        file_size=read32(buf+4);

        j = 0;
        if (!no_names) {
            /* file name entry */
            try("seek to file name info",fseeko(infile,filename_offset+i*0x30,SEEK_SET));
            try("read file name info",fread(buf,1,0x30,infile)!=0x30);
            printf("offset %#10x\tsize %#10x\tname ",file_offset,file_size);
            for (j=0;buf[j] && j<0x20;j++) {
                /*printf("%c",buf[j]);*/
                outfile_name[j]=buf[j];
            }
            outfile_name[j]='\0';
        }

        if (j==0) {
            strcpy(outfile_name,"noname");
        }

        /* see if the file already exists, try alternate names */
        {
            char alternate_outfile_name[0x30];
            int alternate_number = 2;
            FILE * existfile = NULL;
            strcpy(alternate_outfile_name,outfile_name);
            while ((existfile = open_file_with_path(base_name,alternate_outfile_name,'/',"rb"))) {
                /* opened, uh oh, the file already exists */
                fclose(existfile); existfile = NULL;

                snprintf(alternate_outfile_name,0x30,"%s_%d",outfile_name,alternate_number++);
            }
            strcpy(outfile_name,alternate_outfile_name);
        }

        printf("%s\n",outfile_name);
        /*
        printf("\n\tetc ");
        for (j=0x20;j<0x30;j++) printf("%02x",buf[j]);
        */

        /* dump */
        try("open output file",!(outfile=open_file_with_path(base_name,outfile_name,'/',"wb")));
        try("seek to file",fseeko(infile,file_offset,SEEK_SET));
        while (file_size) {
            uint32_t todump = (file_size>=2048?2048:file_size);
            try("read file",fread(buf,1,todump,infile)!=todump);
            try("write file",fwrite(buf,1,todump,outfile)!=todump);
            file_size-=todump;
        }
        try("close outpout file",fclose(outfile));
        outfile=NULL;
    }

    return 0;
}
