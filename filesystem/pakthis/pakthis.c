#include <stdio.h>
#include <string.h>

// pakthis 0.0
// extract from pak files from Megaman X Collection
// by hcs

int main(int argc, char ** argv) {
    FILE * infile, * outfile;
    int i,j,filecount,filesize,fileoff,floc;
    char *t, namebase[257],fname[257+4],buf[4];

    printf("pakthis 0.0\nextract from pack files from Megaman X Collection\n\n");

    if (argc != 2) {printf("usage: %s bob.pak",argv[0]); return 1;}

    infile = fopen(argv[1],"rb");
    if (!infile) {printf("error opening %s\n",argv[1]); return 1;}

    // generate namebase
    t=strrchr(argv[1],'\\');
    if (!t) t=argv[1];
    else t++;
    for (i=0;t<strrchr(argv[1],'.');t++,i++) namebase[i]=*t;
    namebase[i]='\0';

    fread(buf,1,4,infile);

    if (memcmp(buf,"PACK",4)) {printf("%s is not a PACK file\n",argv[1]); return 1;}

    fseek(infile,4,SEEK_CUR);

    fread(&filecount,1,4,infile);

    floc=0x0c;

    for (i=0;i<filecount;i++) {
        fseek(infile,floc,SEEK_SET);
        fread(&fileoff,1,4,infile);
        fread(&filesize,1,4,infile);
        fseek(infile,4,SEEK_CUR);
        floc+=0x0c;
        printf("file #%d\n\toffset: %#08x\n\tsize: %#08x\n",i,fileoff,filesize);

        sprintf(fname,"%s%03d.bin",namebase,i);
        outfile=fopen(fname,"wb");
        if (!outfile) {printf("error opening %s\n",fname); return 1;}

        fseek(infile,fileoff,SEEK_SET);
        for (j=0;j<filesize;j++) {
            fread(buf,1,1,infile);
            fwrite(buf,1,1,outfile);
        }
        fclose(outfile);
    }

    fclose(infile);

    return 0;
}
