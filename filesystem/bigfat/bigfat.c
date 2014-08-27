#include <stdio.h>

// unpack the soundgc.big file (using the soundgc.fat file) in
// Prince of Persia: The Two Thrones (GC)

int main(void) {
    FILE * fat, * big, * outfile;
    char buf[4],namebuf[256];
    unsigned long id,start,size,namelen;
    unsigned long i;

    fat = fopen("soundgc.fat","rb");

    if (!fat) {printf("error opening soundgc.fat\n"); return 1;}

    big = fopen("soundgc.big","rb");

    if (!big) {printf("error opening soundgc.big\n"); return 1;}

    while (fread(&id,1,4,fat)==4) {
        fread(&start,1,4,fat);
        fread(&size,1,4,fat);
        fread(buf,1,4,fat); // ?
        fread(&namelen,1,4,fat);
        if (namelen>256) return 0;
        fread(namebuf,1,namelen,fat);
        printf("%ld %s \t start: %08x\tsize: %ld\n",id,namebuf,start,size);
        if (*((unsigned long*)buf)) printf("***nonzero?\n");

        outfile=fopen(namebuf,"wb");
        // if there's an error assume its because the directory doesn't exist
        if (!outfile) {
            char * t = (char*)strrchr(namebuf,'/');
            *t='\0';
            printf("create dir %s\n",namebuf);
            mkdir(namebuf,0777); // second param for Unix
            *t='/';
            outfile=fopen(namebuf,"wb");
        }
        // catch other errors
        if (!outfile) {printf("error opening %s\n",namebuf); return 1;}

        // copy file
        fseek(big,start,SEEK_SET);
        for (i=0;i<size;i++) {
            fread(buf,1,1,big);
            fwrite(buf,1,1,outfile);
        }

        if (outfile) fclose(outfile);
    }

    fclose(fat);
    fclose(big);

    return 0;
}
