#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "streamfile.h"
#include "util.h"

/*
 * ripping_mama 0.1
 *
 * Unpacks sounds in Cooking Mama brsar. Might work on other 1.4 brsar.
 * No looping or stereo support yet.
 */

#ifdef __MSVCRT__

#define mkdir(name,mode) _mkdir(name)
#define DIR_SEP "\\"

#else

#define DIR_SEP "/"

#endif

#define DEBUG 0

int dump(char * sound_name, char * group_name, char * player_name, off_t file_offset, off_t group_sample_offset, int subfile_number, STREAMFILE * infile);

int main(int argc, char ** argv) {
    STREAMFILE * infile;
    off_t SYMB_off=0, INFO_off=0, FILE_off=0;

    printf("ripping_mama 0.1\n");

    if (argc!=2) {fprintf(stderr,"usage: %s infile.brsar\n",argv[0]); return 1;}

    infile = open_streamfile(argv[1]);

    if (!infile) {fprintf(stderr,"error opening %s for input\n",argv[1]); return 1;}

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
                    break;
                default:
                    printf("\tdon't know what to do with this chunk, skipping\n");
            }
        }
    }


    if (SYMB_off==0 || INFO_off==0 || FILE_off==0) {
        fprintf(stderr,"we expect at least a SYMB, INFO, and FILE chunk in a .brsar\n");
        return 1;
    }

    /* SYMB */
    {
        off_t symbols,sounds,players,groups,banks;
        int i,j;
        size_t symbol_chunk_size = read_32bitBE(SYMB_off-4,infile);
        int symbol_count,sound_count,player_count,group_count,bank_count;

        printf("Symbols:\n");

        symbols = SYMB_off+read_32bitBE(SYMB_off,infile)+4;
        sounds = SYMB_off+read_32bitBE(SYMB_off+4,infile)+8;
        players = SYMB_off+read_32bitBE(SYMB_off+8,infile)+8;
        groups = SYMB_off+read_32bitBE(SYMB_off+12,infile)+8;
        banks = SYMB_off+read_32bitBE(SYMB_off+16,infile)+8;

        symbol_count = read_32bitBE(symbols-4,infile);
        sound_count = read_32bitBE(sounds-4,infile);
        player_count = read_32bitBE(players-4,infile);
        group_count = read_32bitBE(groups-4,infile);
        bank_count = read_32bitBE(banks-4,infile);

        /*
        printf("%d symbols\n",symbol_count);
        for (i=0;i<symbol_count;i++) {
            char * name = read_string_streamfile(
                    SYMB_off+read_32bitBE(symbols+i*4,infile),
                    symbol_chunk_size,infile);
            printf("%5d %s\n",i,name);
            free(name);
        }
        */

        /* I assume that the rest of the data in the symbol table is for binary search or something. */
        for (j=0;j<4;j++) {
            const char * segname;
            off_t segstart;
            int count;
            switch (j) {
                case 0:
                    segname = "sounds";
                    segstart = sounds;
                    count = sound_count;
                    break;
                case 1:
                    segname = "players";
                    segstart = players;
                    count = player_count;
                    break;
                case 2:
                    segname = "groups";
                    segstart = groups;
                    count = group_count;
                    break;
                case 3:
                    segname = "banks";
                    segstart = banks;
                    count = bank_count;
                    break;
            }
            if (count > 1)
                printf("%d %s\n",count/2+1,segname);
            else
                printf("%d %s\n",count,segname);

            /*
            for (i=0;i<count;i++) {
                int name_id;
                char * name;
                off_t off = segstart+i*0x14;

                name_id = read_32bitBE(off+12,infile);
                name = read_string_streamfile(SYMB_off+read_32bitBE(symbols+name_id*4,infile),symbol_chunk_size,infile);
                printf("%4d %s\n",read_32bitBE(off+16,infile),name);
                free(name);

                if (i>0) i++;
            }
            */
        }
    }

    /* INFO */
    {
        off_t sounds,banks,players,files,groups,unk2;
        size_t sound_count,bank_count,player_count,file_count,group_count;
        /* some stuff we need from the symbol table */
        off_t symbols = SYMB_off+read_32bitBE(SYMB_off,infile)+4;
        size_t symbol_chunk_size = read_32bitBE(SYMB_off-4,infile);

        size_t info_chunk_size = read_32bitBE(INFO_off-4,infile);
        int soundno,j;

        printf("Info:\n");

        sounds=INFO_off+read_32bitBE(INFO_off+4,infile);
        sound_count=read_32bitBE(sounds,infile);
        banks=INFO_off+read_32bitBE(INFO_off+0xc,infile);
        bank_count=read_32bitBE(banks,infile);
        players=INFO_off+read_32bitBE(INFO_off+0x14,infile);
        player_count=read_32bitBE(players,infile);
        files=INFO_off+read_32bitBE(INFO_off+0x1c,infile);
        file_count=read_32bitBE(files,infile);
        groups=INFO_off+read_32bitBE(INFO_off+0x24,infile);
        group_count=read_32bitBE(groups,infile);
        unk2=INFO_off+read_32bitBE(INFO_off+0x2c,infile);

        printf("%d sound entries (starting at %#x)\n",sound_count,sounds);
        printf("%d bank entries (starting at %#x)\n",bank_count,banks);
        printf("%d player entries (starting at %#x)\n",player_count,players);
        printf("%d file entries (starting at %#x)\n",file_count,files);
        printf("%d group entries (starting at %#x)\n",group_count,groups);
        printf("something unknown starting at %#x\n",unk2);
        printf("sounds:\n");
        for (soundno=0;soundno<sound_count;soundno++) {
            int fileno;
            int playerno;
            int groupno;
            off_t entry_off;
            off_t file_entry_off;
            off_t player_entry_off;
            off_t groupno_ptr;
            off_t group_entry_off;

            char * sound_name;
            char * player_name;
            char * group_name=NULL;
            char * external_file_name;

            entry_off = INFO_off+read_32bitBE(sounds+8+soundno*8,infile);
                
            sound_name = read_string_streamfile(
                    SYMB_off+read_32bitBE(symbols+read_32bitBE(entry_off,infile)*4,infile),
                    symbol_chunk_size,infile);

            fileno = read_32bitBE(entry_off+4,infile);
            file_entry_off = INFO_off+read_32bitBE(files+8+fileno*8,infile);
            playerno = read_32bitBE(entry_off+8,infile);
            player_entry_off = INFO_off+read_32bitBE(players+8+playerno*8,infile);
            player_name = read_string_streamfile(
                    SYMB_off+read_32bitBE(symbols+read_32bitBE(player_entry_off,infile)*4,infile),
                    symbol_chunk_size,infile);
            /*groupno = read_32bitBE(file_entry_off+0x28,infile);
            if (groupno==0x01000000) {*/
                groupno_ptr = INFO_off+read_32bitBE(file_entry_off+0x24,infile);
                groupno = read_32bitBE(groupno_ptr,infile);
            /*}*/
            group_entry_off = INFO_off+read_32bitBE(groups+8+groupno*8,infile);

#if DEBUG
            /*
            printf("sound entry:\n");
            for (j=0;j<0x44;j++) {
            printf("%02x ",read_8bit(entry_off+j,infile)&0xff);
            if ((j&0xf)==0xf) printf("\n");
            }
            printf("\n");
            */

            /*
            printf("file entry:\n");
            for (j=0;j<0x30;j++) {
                printf("%02x ",read_8bit(file_entry_off+j,infile)&0xff);
                if ((j&0xf)==0xf) printf("\n");
            }

            printf("group entry (%#x):\n",group_entry_off);
            for (j=0;j<0x80*2;j++) {
                printf("%02x ",read_8bit(group_entry_off+j,infile)&0xff);
                if ((j&0xf)==0xf) printf("\n");
            }
            */
#endif

            /*
            printf("file size1: %#x size2: %#x\n",read_32bitBE(file_entry_off+0,infile),read_32bitBE(file_entry_off+4,infile));
            */
            if (read_32bitBE(file_entry_off+0xc,infile)==0x01000000) {
                external_file_name = read_string_streamfile(file_entry_off+0x1c,info_chunk_size,infile);
                printf("%s/%s = %s\n",player_name,sound_name,external_file_name);
                free(external_file_name);
            } else if (groupno==0x01000000) {
                int subfile_number;
                subfile_number = read_32bitBE(entry_off+0x2c,infile);
                printf("%s/%s where?[%d]\n",player_name,sound_name,subfile_number);
            } else {
                off_t file_offset;
                off_t group_sample_offset;
                int subfile_number;
                int i;
                int rwsd_to_skip;
                /* ok, here we should be able to actually *DO SOMETHING* */

                if (read_32bitBE(group_entry_off,infile)>=0) {
                    group_name = read_string_streamfile(
                            SYMB_off+read_32bitBE(symbols+read_32bitBE(group_entry_off,infile)*4,infile),
                            symbol_chunk_size,infile);
                    printf("%s/%s/%s",player_name,group_name,sound_name);
                } else {
                    printf("%s/NOGROUPNAME/%s",player_name,sound_name);
                    group_name=NULL;
                }

                group_sample_offset = read_32bitBE(group_entry_off+0x18,infile);

                file_offset = read_32bitBE(group_entry_off+0x10,infile);

                rwsd_to_skip = read_32bitBE(file_entry_off+0x2c,infile);

                if (read_32bitBE(file_entry_off+0x28,infile)!=0x01000000) {
                    off_t group_subentry_off;
                    group_subentry_off = INFO_off+read_32bitBE(group_entry_off+0x30+rwsd_to_skip*8,infile);
                    /*
                    printf("group sub-entry:\n");
                    for (j=0;j<0x18;j++) {
                        printf("%02x ",read_8bit(group_subentry_off+j,infile)&0xff);
                        if ((j&0xf)==0xf) printf("\n");
                    }
                    */

                    group_sample_offset += read_32bitBE(group_subentry_off+0xc,infile);
                    /*
                       group_sample_offset += read_32bitBE(group_entry_off+0x3c+rwsd_to_skip*0x10,infile);
                       */
                    /*
                    for (i=0;i<rwsd_to_skip;i++) file_offset += read_32bitBE(file_offset+8,infile);
                    */
                    file_offset += read_32bitBE(group_subentry_off+4,infile);
                }

                if (read_32bitBE(entry_off+0x18,infile)!=0x01030000) {
                    printf("\n\tI don't get it ******SKIPPING******\n");
                    continue;
                }
                subfile_number = read_32bitBE(entry_off+0x2c,infile);

#if DEBUG
                printf(" %#x[%d]\n",file_offset,subfile_number);

                printf("RWSD no=%d\n",rwsd_to_skip);
#else
                printf("\n");
#endif

                if (dump(sound_name,group_name,player_name,file_offset,group_sample_offset,subfile_number,infile)) {
                    fprintf(stderr,"dump of %s failed\n",sound_name);
                    return 1;
                }

            }

            if (group_name) free(group_name);
            free(sound_name);
            free(player_name);

        }
    }

    return 0;
}

int dump(char * sound_name, char * group_name, char * player_name, off_t file_offset, off_t group_sample_offset, int subfile_number, STREAMFILE * infile) {
    char file_type[5]={0,0,0,0,0};
    int chunk_count;
    size_t header_size;
    size_t file_size;
    int tabl_subfile_count;
    int tabl_number;

    off_t rwsd_data_off=0;
    int rwsd_data_subfile_count;
    off_t rwsd_data_entry_off;
    int wave_number;
    off_t data_off=0,tabl_off=0;
    off_t data_entry_off,tabl_entry_off;
    off_t rwav_off;
    off_t wave_off=0;
    off_t sample_off=0;

#if DEBUG
    printf("dump(%s, %s, %s, %x, %x, %d)\n",
            sound_name,group_name,player_name,(unsigned)file_offset,
            (unsigned)group_sample_offset,subfile_number);
#endif

    if (read_streamfile(file_type, file_offset, 4, infile)!=4) return 1;
    if (!memcmp(file_type,"RWSD",4)) {
        int i;

        /* RWSD (find what sample this should be) */

        if ((uint32_t)read_32bitBE(file_offset+4,infile)!=0xfeff0103) {fprintf(stderr,"RWSD header version error\n"); return 1;}

        /* scan through RWSD */
        file_size = read_32bitBE(file_offset+8,infile);
        header_size = read_16bitBE(file_offset+0xc,infile);
        chunk_count = read_16bitBE(file_offset+0xe,infile);

        for (i=0;i<chunk_count;i++) {
            size_t chunk_size;
            off_t chunk_off;
            char chunk_type[5]={0,0,0,0,0};

            chunk_off = file_offset+read_32bitBE(file_offset+0x10+i*8,infile);
            chunk_size = read_32bitBE(file_offset+0x10+i*8+4,infile);

            if (read_streamfile(chunk_type, chunk_off, 4, infile)!=4) return 1;

            if (!memcmp(chunk_type,"DATA",4)) rwsd_data_off=chunk_off;

            else {fprintf(stderr,"unknown chunk %s\n",chunk_type); return 1;}
        }

        if (rwsd_data_off==0) {fprintf(stderr,"RWSD missing DATA\n"); return 1;}

        rwsd_data_subfile_count = read_32bitBE(rwsd_data_off+8,infile);
        if (subfile_number<0 || subfile_number >= rwsd_data_subfile_count) {fprintf(stderr,"subfile number %d out of RWSD DATA range [0..%d)\n",subfile_number,rwsd_data_subfile_count); return 1;}

        rwsd_data_entry_off = rwsd_data_off+0x20+read_32bitBE(rwsd_data_off+0x10+subfile_number*8,infile);
        wave_number = read_32bitBE(rwsd_data_entry_off+0x5c,infile);
#if DEBUG
        printf("subfile=%d rwsd_data_off=%#x rwsd_data_entry_off=%#x wave_number=%d\n",subfile_number,rwsd_data_off,rwsd_data_entry_off,wave_number);
#endif

        /* RWAR (find sample) */
        if ((uint32_t)read_32bitBE(group_sample_offset+4,infile)!=0xfeff0100) {fprintf(stderr,"RWAR header version error\n"); return 1;}

        /* scan through RWAR */
        file_size = read_32bitBE(group_sample_offset+8,infile);
        header_size = read_16bitBE(group_sample_offset+0xc,infile);
        chunk_count = read_16bitBE(group_sample_offset+0xe,infile);

        for (i=0;i<chunk_count;i++) {
            size_t chunk_size;
            off_t chunk_off;
            char chunk_type[5]={0,0,0,0,0};

            chunk_off = group_sample_offset+read_32bitBE(group_sample_offset+0x10+i*8,infile);
            chunk_size = read_32bitBE(group_sample_offset+0x10+i*8+4,infile);

            if (read_streamfile(chunk_type, chunk_off, 4, infile)!=4) return 1;

            if (!memcmp(chunk_type,"DATA",4)) data_off=chunk_off;
            else if (!memcmp(chunk_type,"TABL",4)) tabl_off=chunk_off;

            else {fprintf(stderr,"unknown chunk %s\n",chunk_type); return 1;}
        }

        tabl_subfile_count = read_32bitBE(tabl_off+8,infile);

        if (wave_number<0 || wave_number >= tabl_subfile_count) {fprintf(stderr,"subfile number %d out of TABL range [0..%d)\n",wave_number,tabl_subfile_count); return 1;}

        if (data_off==0 || tabl_off==0) {fprintf(stderr,"RWAR missing DATA or TABL\n"); return 1;}

        tabl_entry_off = tabl_off+0xC+wave_number*0xC;
        rwav_off = data_off + read_32bitBE(tabl_entry_off+4,infile);

        {
            char rwav_type[5]={0};
            if (read_streamfile(rwav_type, rwav_off, 4, infile)!=4) return 1;
            if (memcmp(rwav_type,"RWAV",4)) {fprintf(stderr,"didn't find RWAV where I expected to %x\n",rwav_off); return 1;}
            if ((uint32_t)read_32bitBE(rwav_off+4,infile)!=0xfeff0102) {fprintf(stderr,"RWAV header version error\n"); return 1;}
        }

        /* scan through RWAV */
        file_size = read_32bitBE(rwav_off+8,infile);
        header_size = read_16bitBE(rwav_off+0xc,infile);
        chunk_count = read_16bitBE(rwav_off+0xe,infile);

        {
            char dump_buf[0x800];
            off_t offset = 0;
            char *name;
            FILE *outfile;

            if (!group_name) group_name="NOGROUPNAME";
            name = calloc(strlen(player_name)+1+strlen(group_name)+1+strlen(sound_name)+5+1,1);
            sprintf(name,"%s" DIR_SEP "%s",player_name,group_name);
            mkdir(player_name,0777);
            mkdir(name,0777);

            sprintf(name,"%s" DIR_SEP "%s" DIR_SEP "%s.rwav",player_name,group_name,sound_name);
            outfile = fopen(name,"wb");
            if (!outfile) {fprintf(stderr,"error opening %s for writing\n",name); free(name); return 1;}
            free(name);

            while (file_size > 0)
            {
                size_t to_write =
                    (file_size > sizeof(dump_buf)) ? sizeof(dump_buf): file_size;
                if (read_streamfile(dump_buf, rwav_off+offset, to_write, infile)
                        != to_write)
                {
                    fprintf(stderr,"error reading %s for dump\n",name);
                    return 1;
                }
                if (fwrite(dump_buf,1,to_write,outfile) != to_write)
                {
                    fprintf(stderr,"error writing %s\n",name);
                    return 1;
                }
                offset += to_write;
                file_size -= to_write;
            }

            fclose(outfile);
        }

    } else if (!memcmp(file_type,"RSEQ",4)) {
        printf("\tskipping sequence");
        return 0;
    } else {
        fprintf(stderr,"don't know about %s\n",file_type);
        return 1;
    }
    return 0;
}
