#include <stdio.h>

// strip junk from Rally Championship ADPs

int main(int argc, char * * argv) {
    FILE * infile, * outfile;
    char buf[0x20];
    int i;

    if (argc != 3) {printf("usage: %s in.adp out.adp\n",argv[0]); return 1;}

    if (!(infile=fopen(argv[1],"rb"))) {printf("error opening %s\n",argv[1]); return 1;}
    if (!(outfile=fopen(argv[2],"wb"))) {printf("error opening %s\n",argv[2]); return 1;}

    fseek(infile,26,SEEK_SET);

    do {
        for (i=0;i<0x7e0;i+=0x20) {
            if (fread(buf,0x20,1,infile) != 1) break;
            fwrite(buf,0x20,1,outfile);
        }
    } while (fread(buf,0x20,1,infile) == 1);

    fclose(infile);
    fclose(outfile);

    return 0;
}

