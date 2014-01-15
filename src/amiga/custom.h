/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AMIGACUSTOM_H
#define AMIGACUSTOM_H

#include <base/emu.h>

extern void amigaCustomInit();
extern void amigaCustomShutdown();
extern void amigaCustomReset();
extern void amigaCustomSl();

extern void amigaDoCopper();

extern u16 dmacon;
extern u16 intena;
extern u16 intreq;
extern u16 adkcon;

#define dmaen(DMAMASK) (int)((dmacon & DMAMASK) && (dmacon & 0x200))

extern int amigaInputPortButtons[2];
extern uint amigaInputPortDir[2];

extern void INTREQ(u16);
extern void INTREQ_0(u16);
extern u16 INTREQR();

/* maximums for statically allocated tables */

#define MAXHPOS 227
#define MAXVPOS 312

/* PAL/NTSC values */

/* The HRM says: The vertical blanking area (PAL) ranges from line 0 to line 29,
 * and no data can be displayed there. Nevertheless, we lose some overscan data
 * if minfirstline is set to 29. */

#define MAXHPOS_PAL MAXHPOS
#define MAXHPOS_NTSC MAXHPOS
#define MAXVPOS_PAL 312
#define MAXVPOS_NTSC 262
#define MINFIRSTLINE_PAL 42
#define MINFIRSTLINE_NTSC 34
#define VBLANK_ENDLINE_PAL 32
#define VBLANK_ENDLINE_NTSC 24
#define VBLANK_HZ_PAL 50
#define VBLANK_HZ_NTSC 60

extern int maxhpos, maxvpos, minfirstline, numscrlines, vblank_hz;

#define DMA_AUD0      0x0001
#define DMA_AUD1      0x0002
#define DMA_AUD2      0x0004
#define DMA_AUD3      0x0008
#define DMA_DISK      0x0010
#define DMA_SPRITE    0x0020
#define DMA_BLITTER   0x0040
#define DMA_COPPER    0x0080
#define DMA_BITPLANE  0x0100
#define DMA_MASTER    0x0200
#define DMA_BLITPRI   0x0400

#define MAX_WORDS_PER_LINE 50

/* get resolution from bplcon0 */
//#define GET_RES(CON0) (((CON0) & 0x8000) ? RES_HIRES : ((CON0) & 0x40) ? RES_SUPERHIRES : RES_LORES)
#define GET_RES(CON0) (((CON0) & 0x8000) >> 15)
/* get sprite width from FMODE */
#define GET_SPRITEWIDTH(FMODE) ((((FMODE) >> 2) & 3) == 3 ? 64 : (((FMODE) >> 2) & 3) == 0 ? 16 : 32)
/* Compute the number of bitplanes from a value written to BPLCON0  */
#define GET_PLANES(x) ((((x) >> 12) & 7) | (((x) & 0x10) >> 1))

#endif // AMIGACUSTOM_H
