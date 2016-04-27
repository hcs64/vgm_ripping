/*
  MyWAV 0.1.1
  by Luigi Auriemma
  e-mail: aluigi@autistici.org
  web:    aluigi.org

    Copyright 2005,2006 Luigi Auriemma

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

    http://www.gnu.org/licenses/gpl.txt
*/

#include <string.h>
#include <stdint.h>



int mygenh_writehead(FILE *fd, u16 chans, u32 rate, u32 size, u32 flagsandduration, u32 loopoffset, u32 loopsize) {
    mywav_fwmem(fd, "GENH", 4);
    mywav_fwi32(fd, chans);
    mywav_fwi32(fd, 2); // interleave
    mywav_fwi32(fd, rate);
    if (loopsize > 0) {
        mywav_fwi32(fd, loopoffset);
        mywav_fwi32(fd, loopoffset+loopsize);
    } else {
        mywav_fwi32(fd, -1);
        mywav_fwi32(fd, flagsandduration/0x10);
    }
    mywav_fwi32(fd, 3); // 16-bit BE PCM
    mywav_fwi32(fd, 0x38); // genh size
    mywav_fwi32(fd, 0x38); // header size
    int i;

    for (i = 0x24; i < 0x38; i += 4)
        mywav_fwi32(fd, 0);

    return(0);
}

