/*
    Copyright 2005-2011 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl-2.0.txt
*/
// people who supplied xwb files to analyze: john deo, antti

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "show_dump.h"
#include "myxact.h"
#include "mywav.h"
#include "xma_header.h"
/*
  many informations about XWB files are available in xact2wb.h and xact3wb.h from the DirectX SDK
*/

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;

#include "mygenh.h"


#define VER                 "0.3.4"
#define PATHSZ      1024    // 257 was enough, theoretically the system could support 32kb but it's false

    /* SIGNATURES */
#define XWBSIGNi            "WBND"          // intel endian
#define XWBSIGNb            "DNBW"          // network endian
#define WBASIGNi            "HVSI" "WBA\0"  // intel endian
#define WBASIGNb            "ISVH" "\0ABW"  // network endian
#define XSBSIGNi            "SDBK"          // intel endian
#define XSBSIGNb            "KBDS"          // network endian

    /* WAVEBANKMINIWAVEFORMAT */
//#define XWBwFormatTag       (wavebankentry.Format                       & ((1 <<  2) - 1))
//#define XWBnChannels        ((wavebankentry.Format >> (2))              & ((1 <<  3) - 1))
//#define XWBnSamplesPerSec   ((wavebankentry.Format >> (2 + 3))          & ((1 << 18) - 1))
//#define XWBwBlockAlign      ((wavebankentry.Format >> (2 + 3 + 18))     & ((1 <<  8) - 1))
//#define XWBwBitsPerSample   ((wavebankentry.Format >> (2 + 3 + 18 + 8)) & ((1 <<  1) - 1))

    /* FILE OPERATIONS AND OTHER DEFINES */
#define MYFSEEK(x)          if(fseek(fd, file_offset + x, SEEK_SET)) std_err();
#define MAXFILENAME         128                             // 260
#define MAXFILENAMEEXT      (MAXFILENAME + 4)
#define EXECIN              "#FILE"
#define EXECINSZ            (sizeof(EXECIN) - 1)
#define EXECOUTSZ           MAXFILENAMEEXT
#define SHOWFILEOFF         //if(verbose) fprintf(fdinfo, "  current offset    0x%08x\n", (int)ftell(fd));
#define read_file(a,b,c)    if(fread((void *)b, 1, c, a) != (c)) read_err();



u8 *mystrrchrs(u8 *str, u8 *chrs);
int xsb_names(FILE *fd, char *name, int track);
void exec_arg(char *data);
void exec_run(u8 *fname);
void hexdump(FILE *fd, u32 off, u32 len);
void getxwbfile(FILE *fdin, char *fname, u32 size, int codec, int rate, int chans, int expbits, int align, u32 flagsandduration, u32 loopoffset, u32 loopsize);
int xwb_scan_sign(FILE *fd);
int overwrite_file(char *fname);
int get_num(char *data);
void read_err(void);
void write_err(void);
u16 (*fr16)(FILE *fd);
u16 fri16(FILE *fd);
u16 frb16(FILE *fd);
u32 (*fr32)(FILE *fd);
u32 fri32(FILE *fd);
u32 frb32(FILE *fd);
void std_err(void);
void myexit(int ret);



FILE    *fdinfo;
u32     file_offset = 0;
int     execlen     = 0,
        verbose     = 0,
        xsboff      = -1,
        hex_names   = 1;
u8      *tmpexec    = NULL,
        *execstring = NULL;



int main(int argc, char *argv[]) {
    WAVEBANKHEADER  wavebankheader;
    WAVEBANKENTRY   wavebankentry;
    WAVEBANKDATA    wavebankdata;
    FILE    *fd,
            *fdz,
            *fdxsb              = NULL;
    int     i,
            num,
            current_entry,
            unpacklen,
            wavebank_offset,
            waveentry_offset,
            playregion_offset,
            last_segment,
            compact_format      = 0,
            len,
            codec,
            align,
            rate,
            chans,
            bits,
            hexseg              = -1,
            dostdout            = 0,
            list                = 0,
            rawfiles            = 0,
            segidx_entry_name   = 2;
    u8      fname[MAXFILENAMEEXT + 1],
            flags_text[80],
            wbasign[8],
            *outdir             = NULL,
            *entry_name         = NULL,
            *xwbname,
            *codecstr,
            *xsbname            = NULL,
            *ext,
            *p;

    setbuf(stdin,  NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    fputs("\n"
        "XWB/ZWB files unpacker "VER"\n"
        "by Luigi Auriemma\n"
        "e-mail: aluigi@autistici.org\n"
        "web:    aluigi.org\n"
        "\n", stderr);

    if(argc < 2) {
        fprintf(stderr,
            "\n"
            "Usage: %s [options] <file.XWB>\n"
            "\n"
            "Options:\n"
            "-l        lists the files without extracting them\n"
            "-d DIR    output directory where extracting the files\n"
            "-v        verbose output\n"
            "-b N OFF  N is the name of the XSB containing the names of the XWB audio\n"
            "          files and OFF is the offset where these names start\n"
            "-x OFF    offset of the input file for reading the XWB data in it\n"
            "-r \"EXE\"  runs a tool for each output file. EXE is the complete command-line,\n"
            "          use the #FILE pattern which will be substituited with the output file\n"
            "          Example for fast Xbox ADPCM decoding:\n"
            "            unxwb -r \"xbadpdec #FILE #FILE.wav\" music.xwb\n"
            "-o        don't create files, simply dumps them to stdout (probably useless)\n"
            "-s SEG    show the hex content of the segment number SEG (for debugging only)\n"
            "-D        output files in decimal notation (default is hex, 00000123.dat)\n"
            "%s\n"
            "\n", argv[0],
            rawfiles ?  // useful in case I want to change the default mode
            "-a        add header to the output files (by default the tool extracts them in\n"
            "          raw mode exactly as they are stored in the XWB archive" :
            "-R        raw output files (by default the tool adds headers and extensions)");
        myexit(1);
    }

    fdinfo = stdout;

    argc--;
    for(i = 1; i < argc; i++) {
        switch(argv[i][1]) {
            case 'l': list        = 1;                  break;
            case 'd': outdir      = argv[++i];          break;
            case 'v': verbose     = 1;                  break;
            case 'b': {
                xsbname  = argv[++i];
                xsboff   = get_num(argv[++i]);
                break;
            }
            case 'x': file_offset = get_num(argv[++i]); break;
            case 'r': exec_arg(argv[++i]);              break;
            case 'R': rawfiles    = 1;                  break;
            case 'a': rawfiles    = 0;                  break;  // like in fsbext
            case 'o': {
                dostdout = 1;
                fdinfo   = stderr;
                break;
            }
            case 's': hexseg      = get_num(argv[++i]); break;
            case 'D': hex_names   = 0;                  break;
            default: {
                fprintf(stderr, "\nError: wrong command-line argument (%s)\n\n", argv[i]);
                myexit(1);
            }
        }
    }

    xwbname = argv[argc];

    if(!strcmp(xwbname, "-")) {
        fprintf(fdinfo, "- open file         %s\n", "stdin");
        fd = stdin;
    } else {
        fprintf(fdinfo, "- open file         %s\n", xwbname);
        fd = fopen(xwbname, "rb");
        if(!fd) std_err();
    }

        /* COMPATIBILITY INITIALIZATION */

    fr16 = fri16;
    fr32 = fri32;

    memset(&wavebankheader, 0, sizeof(wavebankheader));
    memset(&wavebankentry,  0, sizeof(wavebankentry));
    memset(&wavebankdata,   0, sizeof(wavebankdata));

    ext = strrchr(xwbname, '.');
    if(ext) ext++;

    if(rawfiles) {
        fprintf(fdinfo, "- the files will be extracted in raw mode\n");
    } else {
        fprintf(fdinfo, "- the tool will try to add a header to the extracted files\n");
    }

        /* CHECK FOR SXB FILES */

    if(ext && (!stricmp(ext, "sxb") || !stricmp(ext, "vxb"))) {
        if(fr32(fd) >> 24) {     // lame endian checker
            fr16 = frb16;
            fr32 = frb32;
        }
        file_offset += fr32(fd);
    }
    MYFSEEK(0);

        /* CHECK FOR WBA SIGNATURE */

    read_file(fd, wbasign, sizeof(wbasign));
    if(!memcmp(wbasign, WBASIGNi, sizeof(WBASIGNi) - 1) ||
       !memcmp(wbasign, WBASIGNb, sizeof(WBASIGNb) - 1)) {
        file_offset += 4096;
    }
    MYFSEEK(0);

        /* XSB */

/* no longer automatic
    if(!xsbname && ext) {
        strcpy(ext, "xsb");
        xsbname = xwbname;
    }
*/
    if(xsbname) {
        fprintf(fdinfo, "- open XSB file     %s\n", xsbname);
        fdxsb = fopen(xsbname, "rb");
        if(!fdxsb) fprintf(fdinfo, "- XSB file not found\n");
    }

    xsb_names(fdxsb, fname, -1);

        /* OUTPUT FOLDER */

    if(outdir) {
        fprintf(fdinfo, "- change directory  %s\n", outdir);
        if(chdir(outdir) < 0) std_err();
    }

        /* SIGNATURE */

check_signature:
    read_file(fd, wavebankheader.dwSignature, 4);
    if(verbose) fprintf(fdinfo, "- signature         %.4s\n", wavebankheader.dwSignature);

    if(!memcmp(wavebankheader.dwSignature, XWBSIGNi, sizeof(XWBSIGNi) - 1)) {
        if(verbose) fprintf(fdinfo, "- little/intel endian values\n");
        fr16 = fri16;
        fr32 = fri32;

    } else if(!memcmp(wavebankheader.dwSignature, XWBSIGNb, sizeof(XWBSIGNb) - 1)) {
        if(verbose) fprintf(fdinfo, "- big/network endian values\n");
        fr16 = frb16;
        fr32 = frb32;

    } else {
        fprintf(fdinfo, "  alert: the sign is invalid, now I scan the file for the needed signature\n");
        fseek(fd, -4, SEEK_CUR);
        len = xwb_scan_sign(fd);
        if(len < 0) {
            fprintf(stderr, "\nError: no signature found after scanning, this file is not a XWB file\n\n");
            fclose(fd);
            myexit(1);
        }
        file_offset += len;
        fprintf(fdinfo, "- found possible signature at offset 0x%08x\n", file_offset);
        MYFSEEK(0);
        goto check_signature;
    }

        /* VERSION */

    wavebankheader.dwVersion = fr32(fd);
    if(verbose) fprintf(fdinfo, "- version           %u\n", wavebankheader.dwVersion);

        /* SEGMENTS */

    last_segment = 4;
    if(wavebankheader.dwVersion == 1)  goto WAVEBANKDATA_goto;
    if(wavebankheader.dwVersion <= 3)  last_segment = 3;
    if(wavebankheader.dwVersion >= 42) fr32(fd);    // skip dwHeaderVersion

    SHOWFILEOFF;
    for(i = 0; i <= last_segment; i++) {        // WAVEBANKREGION
        wavebankheader.Segments[i].dwOffset = fr32(fd);
        wavebankheader.Segments[i].dwLength = fr32(fd);

        if(verbose) {
            fprintf(fdinfo, "- segment %u         offset 0x%08x   length %u\n",
                i, wavebankheader.Segments[i].dwOffset, wavebankheader.Segments[i].dwLength);
        }
    }

        /* WAVEBANKDATA */

    MYFSEEK(wavebankheader.Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset);

WAVEBANKDATA_goto:
    SHOWFILEOFF;

    wavebankdata.dwFlags                        = fr32(fd);
    wavebankdata.dwEntryCount                   = fr32(fd);
    if((wavebankheader.dwVersion == 2) || (wavebankheader.dwVersion == 3)) {
        read_file(fd, wavebankdata.szBankName, 16); // version 1 and 2 want 16 bytes
    } else {
        read_file(fd, wavebankdata.szBankName, sizeof(wavebankdata.szBankName));
    }
    if(wavebankheader.dwVersion == 1) {
        wavebank_offset                         = (int)ftell(fd) - file_offset;
        wavebankdata.dwEntryMetaDataElementSize = 20;
    } else {
        wavebankdata.dwEntryMetaDataElementSize = fr32(fd);
        wavebankdata.dwEntryNameElementSize     = fr32(fd);
        wavebankdata.dwAlignment                = fr32(fd);
        wavebank_offset                         = wavebankheader.Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset;
    }

    if(wavebankdata.dwFlags & WAVEBANK_FLAGS_COMPACT) {
        compact_format                          = fr32(fd);
    }

    flags_text[0] = 0;
    if(wavebankdata.dwFlags & WAVEBANK_TYPE_BUFFER)         strcat(flags_text, "in-memory, ");
    if(wavebankdata.dwFlags & WAVEBANK_TYPE_STREAMING)      strcat(flags_text, "streaming, ");
    if(wavebankdata.dwFlags & WAVEBANK_FLAGS_ENTRYNAMES)    strcat(flags_text, "bank+entry_names, ");
    if(wavebankdata.dwFlags & WAVEBANK_FLAGS_COMPACT)       strcat(flags_text, "compact_format, ");
    if(wavebankdata.dwFlags & WAVEBANK_FLAGS_SYNC_DISABLED) strcat(flags_text, "disabled_bank, ");
    if(flags_text[0]) flags_text[strlen(flags_text) - 2] = 0;   // remove commas

    if(verbose) {
        fprintf(fdinfo, "\n"
            "- flags             %s\n"
            "- files             %u\n"
            "- bank name         %.*s\n"
            "- entry meta size   %u\n"
            "- entry name size   %u\n"
            "- alignment         %u\n",
            flags_text,
            wavebankdata.dwEntryCount,
            sizeof(wavebankdata.szBankName), wavebankdata.szBankName,
            wavebankdata.dwEntryMetaDataElementSize,
            wavebankdata.dwEntryNameElementSize,
            wavebankdata.dwAlignment);
    }

        /* COMPATIBILITY WORK-AROUNDS, DEBUGGING and ALLOCATION */

    playregion_offset = wavebankheader.Segments[last_segment].dwOffset;
    if(!playregion_offset) {
        playregion_offset =
            wavebank_offset +
            (wavebankdata.dwEntryCount * wavebankdata.dwEntryMetaDataElementSize);
    }

    if(verbose && (wavebankdata.dwEntryMetaDataElementSize < 24)) {
        fprintf(fdinfo, "- dwEntryMetaDataElementSize is small\n");
    }

    if((hexseg >= 0) && (hexseg <= last_segment)) {
        hexdump(fd, wavebankheader.Segments[hexseg].dwOffset, wavebankheader.Segments[hexseg].dwLength);
        fclose(fd);
        myexit(0);
        return(0);
    } else if(hexseg != -1) {
        myexit(0);
        return(0);
    }

    if(wavebankheader.dwVersion >= 42) segidx_entry_name = 3;

    waveentry_offset = wavebankheader.Segments[segidx_entry_name].dwOffset;
    if(wavebankheader.Segments[segidx_entry_name].dwOffset && wavebankheader.Segments[segidx_entry_name].dwLength) {
        if(wavebankdata.dwEntryNameElementSize == -1) wavebankdata.dwEntryNameElementSize = 0;
        entry_name = malloc(wavebankdata.dwEntryNameElementSize + 1);
        if(!entry_name) std_err();
        entry_name[wavebankdata.dwEntryNameElementSize] = 0;
    }

        /* WAVEBANKENTRY */

    fprintf(fdinfo, "\n"
        "  length      fmt   freq c b  filename\n"
        "=====================================================================\n");

    for(current_entry = 0; current_entry < wavebankdata.dwEntryCount; current_entry++) {
        MYFSEEK(wavebank_offset);
        SHOWFILEOFF;

        memset(&wavebankentry, 0, sizeof(wavebankentry));

        if(wavebankdata.dwFlags & WAVEBANK_FLAGS_COMPACT) {
            len = fr32(fd);
            wavebankentry.Format              = compact_format;
            wavebankentry.PlayRegion.dwOffset = (len & ((1 << 21) - 1)) * wavebankdata.dwAlignment;
            wavebankentry.PlayRegion.dwLength = (len >> 21) & ((1 << 11) - 1);

                /* WORK-AROUND BECAUSE I DON'T KNOW HOW TO HANDLE THE DEVIATION LENGTH! */

            MYFSEEK(wavebank_offset + wavebankdata.dwEntryMetaDataElementSize); // seek to the next
            if(current_entry == (wavebankdata.dwEntryCount - 1)) {              // the last track
                len = wavebankheader.Segments[last_segment].dwLength;
            } else {
                len = ((fr32(fd) & ((1 << 21) - 1)) * wavebankdata.dwAlignment);
            }
            wavebankentry.PlayRegion.dwLength =
                len -                               // next offset
                wavebankentry.PlayRegion.dwOffset;  // current offset
            goto wavebank_handle;
        }

        if(wavebankheader.dwVersion == 1) {
            wavebankentry.Format              = fr32(fd);
            wavebankentry.PlayRegion.dwOffset = fr32(fd);
            wavebankentry.PlayRegion.dwLength = fr32(fd);
            wavebankentry.LoopRegion.dwOffset = fr32(fd);
            wavebankentry.LoopRegion.dwLength = fr32(fd);
        } else {
            if(wavebankdata.dwEntryMetaDataElementSize >=  4) wavebankentry.dwFlagsAndDuration  = fr32(fd);
            if(wavebankdata.dwEntryMetaDataElementSize >=  8) wavebankentry.Format              = fr32(fd);
            if(wavebankdata.dwEntryMetaDataElementSize >= 12) wavebankentry.PlayRegion.dwOffset = fr32(fd);
            if(wavebankdata.dwEntryMetaDataElementSize >= 16) wavebankentry.PlayRegion.dwLength = fr32(fd);
            if(wavebankdata.dwEntryMetaDataElementSize >= 20) wavebankentry.LoopRegion.dwOffset = fr32(fd);
            if(wavebankdata.dwEntryMetaDataElementSize >= 24) wavebankentry.LoopRegion.dwLength = fr32(fd);
        }

        if(wavebankdata.dwEntryMetaDataElementSize < 24) {                              // work-around
            if(!wavebankentry.PlayRegion.dwLength) {
                wavebankentry.PlayRegion.dwLength = wavebankheader.Segments[last_segment].dwLength;
            }

        } else if(wavebankdata.dwEntryMetaDataElementSize > sizeof(wavebankentry)) {    // skip unused fields
            MYFSEEK(wavebank_offset + wavebankdata.dwEntryMetaDataElementSize);
        }

wavebank_handle:
        wavebank_offset                   += wavebankdata.dwEntryMetaDataElementSize;
        wavebankentry.PlayRegion.dwOffset += playregion_offset;

        if(wavebankheader.dwVersion == 1) {         // I'm not 100% sure if the following is correct
            // version 1:
            // 1 00000000 000101011000100010 0 001 0
            // | |         |                 | |   |
            // | |         |                 | |   wFormatTag
            // | |         |                 | nChannels
            // | |         |                 ???
            // | |         nSamplesPerSec
            // | wBlockAlign
            // wBitsPerSample

            codec = (wavebankentry.Format                        ) & ((1 <<  1) - 1);
            chans = (wavebankentry.Format >> (1)                 ) & ((1 <<  3) - 1);
            rate  = (wavebankentry.Format >> (1 + 3 + 1)         ) & ((1 << 18) - 1);
            align = (wavebankentry.Format >> (1 + 3 + 1 + 18)    ) & ((1 <<  8) - 1);
            bits  = (wavebankentry.Format >> (1 + 3 + 1 + 18 + 8)) & ((1 <<  1) - 1);

        /*} else if(wavebankheader.dwVersion == 23) { // I'm not 100% sure if the following is correct
            // version 23:
            // 1000000000 001011101110000000 001 1
            // | |        |                  |   |
            // | |        |                  |   ???
            // | |        |                  nChannels?
            // | |        nSamplesPerSec
            // | ???
            // !!!UNKNOWN FORMAT!!!

            //codec = -1;
            //chans = (wavebankentry.Format >>  1) & ((1 <<  3) - 1);
            //rate  = (wavebankentry.Format >>  4) & ((1 << 18) - 1);
            //bits  = (wavebankentry.Format >> 31) & ((1 <<  1) - 1);
            codec = (wavebankentry.Format                    ) & ((1 <<  1) - 1);
            chans = (wavebankentry.Format >> (1)             ) & ((1 <<  3) - 1);
            rate  = (wavebankentry.Format >> (1 + 3)         ) & ((1 << 18) - 1);
            align = (wavebankentry.Format >> (1 + 3 + 18)    ) & ((1 <<  9) - 1);
            bits  = (wavebankentry.Format >> (1 + 3 + 18 + 9)) & ((1 <<  1) - 1); */

        } else {            // versions 2, 3, 37, 42, 43, 44 and so on, check WAVEBANKMINIWAVEFORMAT in xact3wb.h
            // 0 00000000 000111110100000000 010 01
            // | |        |                  |   |
            // | |        |                  |   wFormatTag
            // | |        |                  nChannels
            // | |        nSamplesPerSec
            // | wBlockAlign
            // wBitsPerSample

            codec = (wavebankentry.Format                    ) & ((1 <<  2) - 1);
            chans = (wavebankentry.Format >> (2)             ) & ((1 <<  3) - 1);
            rate  = (wavebankentry.Format >> (2 + 3)         ) & ((1 << 18) - 1);
            align = (wavebankentry.Format >> (2 + 3 + 18)    ) & ((1 <<  8) - 1);
            bits  = (wavebankentry.Format >> (2 + 3 + 18 + 8)) & ((1 <<  1) - 1);
        }

            /* TRY XSB NAME */

        len = xsb_names(fdxsb, fname, current_entry);

            /* CODEC / FORMAT */

        if(rawfiles) codec = -1;
        switch(codec) {
            case WAVEBANKMINIFORMAT_TAG_PCM: {
                strcpy(fname + len, ".genh");
                codecstr = "PCM";
                break;
            }
            case WAVEBANKMINIFORMAT_TAG_XMA: {
                strcpy(fname + len, ".wav");
                codecstr = "XMA";
                break;
            }
            case WAVEBANKMINIFORMAT_TAG_ADPCM: {
                strcpy(fname + len, ".wav");
                codecstr = "ADP";
                break;
            }
            case WAVEBANKMINIFORMAT_TAG_WMA: {
                strcpy(fname + len, ".wma");
                codecstr = "WMA";
                break;
            }
            default: {
                strcpy(fname + len, ".dat");
                codecstr = "???";
                break;
            }
        }

        fprintf(fdinfo,
            "  %-10u  %-3s %6u %u %-2u %s\n",
            wavebankentry.PlayRegion.dwLength,
            codecstr,
            rate,
            chans,
            bits ? 16 : 8,
            fname);

        if(verbose) {
            if(entry_name) {
                MYFSEEK(waveentry_offset);
                waveentry_offset += wavebankdata.dwEntryNameElementSize;
                read_file(fd, entry_name, wavebankdata.dwEntryNameElementSize);
                fprintf(fdinfo, "              description     %s\n", entry_name);
            }

            fprintf(fdinfo,
                "  0x%08x  format          0x%08x   flags           0x%08x\n"
                "              region offset   0x%08x   region length   0x%08x\n"
                "\n",
                file_offset + wavebankentry.PlayRegion.dwOffset, wavebankentry.Format, wavebankentry.dwFlagsAndDuration,
                file_offset + wavebankentry.LoopRegion.dwOffset, wavebankentry.LoopRegion.dwLength);
        }

            /* FILE EXTRACTION */

        if(!list) {
            MYFSEEK(wavebankentry.PlayRegion.dwOffset);

            if(execstring && dostdout)  exec_run(fname);    // start before if stdout (not used!!!)

            getxwbfile(
                fd,
                dostdout ? NULL : fname,
                wavebankentry.PlayRegion.dwLength,
                codec,
                rate,
                chans,
                bits,
                align,
                wavebankentry.dwFlagsAndDuration,
                wavebankentry.LoopRegion.dwOffset,
                wavebankentry.LoopRegion.dwLength
                );

            if(execstring && !dostdout) exec_run(fname);    // start later if not
        }
    }

    if(entry_name) free(entry_name);
    if(fdxsb) fclose(fdxsb);
    fclose(fd);
    fprintf(fdinfo, "\n- finished (%u files)\n\n", wavebankdata.dwEntryCount);
    myexit(0);
    return(0);
}



u8 *mystrrchrs(u8 *str, u8 *chrs) {
    int     i;
    u8      *p,
            *ret = NULL;

    if(str) {
        for(i = 0; chrs[i]; i++) {
            p = strrchr(str, chrs[i]);
            if(p) {
                str = p;
                ret = p;
            }
        }
    }
    return(ret);
}



int xsb_names(FILE *fd, char *name, int track) {
    int     i;
    char    *p;

    if(!fd) goto errorx;
    if(track >= 0) goto return_names;

    if(verbose) {
        fseek(fd, xsboff, SEEK_SET);

        for(i = 0; ; i++) {
            for(p = name; fread(p, 1, 1, fd); p++) {
                if(!p || (*p < ' ')) break;
            }
            *p = 0;
            if(p == name) break;

            fprintf(fdinfo, "  track %04x        %s\n", i, name);
        }

        fputc('\n', fdinfo);
    }

    return(0);

return_names:
    fseek(fd, xsboff, SEEK_SET);

    for(i = 0; ; i++) {
        for(p = name; fread(p, 1, 1, fd); p++) {
            if(!p || (*p < ' ')) break;
        }
        *p = 0;
        if(p == name) break;

        if(i == track) return(strlen(name));
    }

errorx:
    return(sprintf(name, hex_names ? "%08x" : "%u", track));
}



void exec_arg(char *data) {
    int     i;
    u8      *p;

    execstring = data;
    for(p = execstring, i = 0; (p = strstr(p, EXECIN)); p++, i++);

    tmpexec = malloc(strlen(execstring) - (i * EXECINSZ) + (i * EXECOUTSZ) + 1);
    if(!tmpexec) std_err();
}



void exec_run(u8 *fname) {
    int     fnamelen;
    u8      *p,
            *l,
            *execptr;

    fnamelen = strlen(fname);
    execptr  = tmpexec;

    for(p = execstring; (l = strstr(p, EXECIN)); p = l + EXECINSZ) {
        memcpy(execptr, p, l - p);
        execptr += l - p;
        memcpy(execptr, fname, fnamelen);
        execptr += fnamelen;
    }
    strcpy(execptr, p);

    fprintf(fdinfo, "   Execute: \"%s\"\n", tmpexec);
    system(tmpexec);
}



void hexdump(FILE *fd, u32 off, u32 len) {
    int     t;
    u8      buff[512];

    fprintf(fdinfo, "- hex dump of %u bytes at offset 0x%08x\n", len, off);

    MYFSEEK(off);

    for(t = sizeof(buff); len; len -= t) {
        if(len < t) t = len;
        if(fread(buff, 1, t, fd) != t) break;
        show_dump(buff, t, stdout);
    }
}



void getxwbfile(FILE *fdin, char *fname, u32 size, int codec, int rate, int chans, int expbits, int align, u32 flagsandduration, u32 loopoffset, u32 loopsize) {
    mywav_fmtchunk  fmt;
    FILE    *fdo;
    u32     pos;
    int     len;
    u8      buff[8192];

    if(codec == WAVEBANKMINIFORMAT_TAG_WMA) {
        len = fread(buff, 1, 16, fdin);
        if(len == 16) {
            fseek(fdin, -len, SEEK_CUR);
            if(memcmp(buff, "\x30\x26\xb2\x75\x8e\x66\xcf\x11\xa6\xd9\x00\xaa\x00\x62\xce\x6c", len)) {
                codec = WAVEBANKMINIFORMAT_TAG_XMA;
                if(fname) strcpy(strrchr(fname, '.'), ".genh");
            }
        }
    }

    if(fname) {
        if(!overwrite_file(fname)) return;
        fdo = fopen(fname, "wb");
        if(!fdo) std_err();
    } else {
        fdo = stdout;
    }

    if(chans <= 0) chans = 1;   // useless?

    switch(codec) {
        case WAVEBANKMINIFORMAT_TAG_PCM: {
            /*fmt.wFormatTag       = 0x0001;
            fmt.wChannels        = chans;
            fmt.dwSamplesPerSec  = rate;
            fmt.wBitsPerSample   = 8 << expbits;
            fmt.wBlockAlign      = (fmt.wBitsPerSample / 8) * fmt.wChannels;
            fmt.dwAvgBytesPerSec = fmt.dwSamplesPerSec * fmt.wBlockAlign;
            mywav_writehead(fdo, &fmt, size, NULL, 0);*/
            mygenh_writehead(fdo, chans, rate, size, flagsandduration, loopoffset, loopsize);
            break;
        }
        case WAVEBANKMINIFORMAT_TAG_WMA: {
            // WMA is ready to play
            break;
        }
        case WAVEBANKMINIFORMAT_TAG_XMA: {
            /*fmt.wFormatTag       = 0x0069;
            fmt.wChannels        = chans;
            fmt.dwSamplesPerSec  = rate;
            fmt.wBitsPerSample   = 4;
            fmt.wBlockAlign      = 36 * fmt.wChannels;
            fmt.dwAvgBytesPerSec = (689 * fmt.wBlockAlign) + 4; // boh, not important
            mywav_writehead(fdo, &fmt, size, "\x02\x00" "\x40\x00", 4); // useless*/
            xma2_header(fdo, rate, chans, 16, size, NULL, 0, 0);    // samples?
            break;
        }
        case WAVEBANKMINIFORMAT_TAG_ADPCM: {
            fmt.wFormatTag       = 0x0002;
            fmt.wChannels        = chans;
            fmt.dwSamplesPerSec  = rate;
            fmt.wBitsPerSample   = 4;
            fmt.wBlockAlign      = (align + ADPCM_MINIWAVEFORMAT_BLOCKALIGN_CONVERSION_OFFSET) * fmt.wChannels;
            fmt.dwAvgBytesPerSec = 21 * fmt.wBlockAlign;   // should be correct, although not much important
            mywav_writehead(fdo, &fmt, size, NULL, 0);
            break;
        }
        default: {
            break;
        }
    }

    for(len = sizeof(buff); size; size -= len) {
        if(len > size) len = size;
        if(fread(buff, 1, len, fdin) != len) read_err();
        if(fwrite(buff, 1, len, fdo) != len) write_err();
    }

    if(fname) fclose(fdo);
}



int xwb_scan_sign(FILE *fd) {
    int     len,
            tot;
    u8      buff[2048],
            *p,
            *l;

    for(tot = 0; (len = fread(buff, 1, sizeof(buff), fd)); tot += len) {
        for(p = buff, l = buff + len - 8; p < l; p++) {
            /* here I check the XWB signature plus the last version's byte */
            if(
              (!memcmp(p, XWBSIGNi, 4) && !p[7])  ||
              (!memcmp(p, XWBSIGNb, 4) && !p[4])) {
                return(tot + (p - buff));
            }
        }
    }
    return(-1);
}



int overwrite_file(char *fname) {
    FILE    *fd;
    int     t;

    fd = fopen(fname, "rb");
    if(!fd) return(1);
    fclose(fd);
    fprintf(fdinfo, "- do you want to overwrite the file \"%s\"? (y/N): ", fname);
    fflush(stdin);
    for(;;) {
        t = tolower(fgetc(stdin));
        if(t < ' ') continue;
        if(t == 'y') return(1);
        break;
    }
    return(0);
}



int get_num(char *data) {
    int     ret;

    if((data[0] == '0') && (tolower(data[1]) == 'x')) {
        sscanf(data + 2, "%x", &ret);
    } else if(data[0] == '$') {
        sscanf(data + 1, "%x", &ret);
    } else {
        sscanf(data, "%d", &ret);
    }
    return(ret);
}



void read_err(void) {
    fprintf(stderr, "\nError: the file contains unexpected data\n\n");
    myexit(1);
}



void write_err(void) {
    fprintf(stderr, "\nError: impossible to write the output file, probably your disk space is finished\n\n");
    myexit(1);
}



u16 fri16(FILE *fd) {
    int     t1,
            t2;

    t1 = fgetc(fd);
    t2 = fgetc(fd);
    if((t1 < 0) || (t2 < 0)) read_err();
    return(t1 | (t2 << 8));
}



u16 frb16(FILE *fd) {
    int     t1,
            t2;

    t1 = fgetc(fd);
    t2 = fgetc(fd);
    if((t1 < 0) || (t2 < 0)) read_err();
    return(t2 | (t1 << 8));
}



u32 fri32(FILE *fd) {
    int     t1,
            t2,
            t3,
            t4;

    t1 = fgetc(fd);
    t2 = fgetc(fd);
    t3 = fgetc(fd);
    t4 = fgetc(fd);
    if((t1 < 0) || (t2 < 0) || (t3 < 0) || (t4 < 0)) read_err();
    return(t1 | (t2 << 8) | (t3 << 16) | (t4 << 24));
}



u32 frb32(FILE *fd) {
    int     t1,
            t2,
            t3,
            t4;

    t1 = fgetc(fd);
    t2 = fgetc(fd);
    t3 = fgetc(fd);
    t4 = fgetc(fd);
    if((t1 < 0) || (t2 < 0) || (t3 < 0) || (t4 < 0)) read_err();
    return(t4 | (t3 << 8) | (t2 << 16) | (t1 << 24));
}



void std_err(void) {
    perror("\nError");
    myexit(1);
}



void myexit(int ret) {
    exit(ret);
}


