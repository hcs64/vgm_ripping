#include <stdio.h>
#include <string.h>

char been[65536];

int main(int argc, char ** argv) {
	int i;
	int xor;
	int xorstart,xormult,xoradd;

	if (argc!=4) {printf("Usage: %s start mult add\n",argv[0]); return 1;}

	memset(been,0,sizeof(been));

	sscanf(argv[1],"%x",&xorstart);
	sscanf(argv[2],"%x",&xormult);
	sscanf(argv[3],"%x",&xoradd);

	for (i=0,xor=xorstart;!been[xor];i++) {
		printf("%d\t%04x\n",i,xor);
		been[xor]=1;
		xor=((xor*xormult)+xoradd)&0x7fff;
	}
	printf("%d\t%04x\n",i,xor);
}
