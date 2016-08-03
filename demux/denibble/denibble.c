#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FILE *infile, *outfileL, *outfileR;
    if (argc != 4) exit(EXIT_FAILURE);

    infile = fopen(argv[1],"rb");
    outfileL = fopen(argv[2],"wb");
    outfileR = fopen(argv[3],"wb");

    if (!infile || !outfileL || !outfileR) exit(EXIT_FAILURE);

    do {
        unsigned char c[2];
        unsigned char l,r;
        size_t s;
        s = fread(c,1,2,infile);
        if (s != 2)
        {
            if (feof(infile)) break;
            perror("fwrite");
            exit(EXIT_FAILURE);
        }

        l = ((c[0]&0x0f) << 4) | (c[1]&0x0f);
        r = (c[0]&0xf0) | ((c[1]&0xf0) >> 4);
        s = fwrite(&l,1,1,outfileL);
        if (s != 1)
        {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
        s = fwrite(&r,1,1,outfileR);
        if (s != 1)
        {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
    } while (!feof(infile));

    fclose(infile);
    fclose(outfileL);
    fclose(outfileR);

    printf("ok\n");
}
