#include <stdio.h>
#include <string.h>

// extract files from Batman Begins (GC)

int main(void) {
    long i,loc,len,ver;
    size_t l;
    FILE * infile, * outfile, * idxfile;
    char buf[1025],fname[1025],*ext,*fn;
    infile = fopen("Filelist.000","rb");
    if (!infile) return 1;
    idxfile = fopen("Filelist.txt","r");
    if (!idxfile) return 1;

    while (fgets(buf,1024,idxfile)) {
        fscanf(idxfile,"%s %*s %*s %li %*s %*s %d %*s %*s %*s %*s %*s %*s %*s %*s %x",fname,&len,&ver,&loc);

        ext = strrchr(fname,'.')+1;
        fn = strrchr(fname,'\\')+1;

        if (!strcmp(ext,"sfx")) {
            printf("%s len: %li loc: %x\n",fn,len,loc);

            outfile=fopen(fn,"wb");

            fseek(infile,loc,SEEK_SET);

            for (i=0;i<len;i++) {
                fread(&buf,1,1,infile);
                fwrite(&buf,1,1,outfile);
            }
            fclose(outfile);
        }
        
    }

    fclose(infile);
    fclose(idxfile);
}
