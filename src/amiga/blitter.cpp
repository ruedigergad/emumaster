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

#include "events.h"
#include "mem.h"
#include "custom.h"
#include "cpu.h"
#include "blitter.h"
#include "blit.h"

static __inline__ void setNasty() {
	amigaCpuReleaseTimeslice();
	amigaCpuSetSpcFlag(SpcFlagBltNasty);
}

u16 bltcon0,bltcon1;
u32 bltapt,bltbpt,bltcpt,bltdpt;

int blinea_shift;
static u16 blitlpos, blinea, blineb;
static u32 bltcnxlpt,bltdnxlpt;
static int blitline,blitfc,blitfill,blitife,blitdesc,blitsing;
static int blitonedot,blitsign;

struct bltinfo blt_info;

static u8 blit_filltable[256][4][2];
u32 blit_masktable[BLITTER_MAX_WORDS];
enum blitter_states bltstate;

void build_blitfilltable(void)
{
    unsigned int d, fillmask;
    int i;

    for (i = BLITTER_MAX_WORDS; i--;)
	blit_masktable[i] = 0xFFFF;

    for (d = 0; d < 256; d++) {
	for (i = 0; i < 4; i++) {
	    int fc = i & 1;
		u8 data = d;
	    for (fillmask = 1; fillmask != 0x100; fillmask <<= 1) {
		u16 tmp = data;
		if (fc) {
		    if (i & 2)
			data |= fillmask;
		    else
			data ^= fillmask;
		}
		if (tmp & fillmask) fc = !fc;
	    }
	    blit_filltable[d][i][0] = data;
	    blit_filltable[d][i][1] = fc;
	}
    }
}

static void blitter_dofast(void)
{
    int i,j;
	u32 bltadatptr = 0, bltbdatptr = 0, bltcdatptr = 0, bltddatptr = 0;
	u8 mt = bltcon0 & 0xFF;

    blit_masktable[BLITTER_MAX_WORDS - 1] = blt_info.bltafwm;
    blit_masktable[BLITTER_MAX_WORDS - blt_info.hblitsize] &= blt_info.bltalwm;

#ifdef DEBUG_BLITTER
    dbgf("blitter_dofast bltafwm=0x%X, bltcon0=0x%X\n",blt_info.bltafwm,bltcon0);
#endif
    if (bltcon0 & 0x800) {
	bltadatptr = bltapt;
	bltapt += ((blt_info.hblitsize*2) + blt_info.bltamod)*blt_info.vblitsize;
    }
    if (bltcon0 & 0x400) {
	bltbdatptr = bltbpt;
	bltbpt += ((blt_info.hblitsize*2) + blt_info.bltbmod)*blt_info.vblitsize;
    }
    if (bltcon0 & 0x200) {
	bltcdatptr = bltcpt;
	bltcpt += ((blt_info.hblitsize*2) + blt_info.bltcmod)*blt_info.vblitsize;
    }
    if (bltcon0 & 0x100) {
	bltddatptr = bltdpt;
	bltdpt += ((blt_info.hblitsize*2) + blt_info.bltdmod)*blt_info.vblitsize;
    }

#ifdef DEBUG_BLITTER
    print_bltinfo(&blt_info);
#endif

    if (blitfunc_dofast[mt] && !blitfill)
    {
#ifdef DEBUG_BLITTER
	dbgf("blitfunc_dofast[%i](0x%X,0x%X,0x%X,0x%X)\n",mt,bltadatptr, bltbdatptr, bltcdatptr, bltddatptr);
#endif
	(*blitfunc_dofast[mt])(bltadatptr, bltbdatptr, bltcdatptr, bltddatptr, &blt_info);
    }
    else {
	u32 blitbhold = blt_info.bltbhold;
	u32 preva = 0, prevb = 0;
	u32 dstp = 0;
	u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - blt_info.hblitsize;

#ifdef DEBUG_BLITTER
	dbgf("bltbhold=0x%X, vblitsize=0x%X, bltcon1=0x%X\n",blt_info.bltbhold,blt_info.vblitsize,bltcon1);
#endif
	/*if (!blitfill) qDebug ("minterm %x not present\n",mt); */
	for (j = blt_info.vblitsize; j--;) {
	    blitfc = !!(bltcon1 & 0x4);
	    for (i = blt_info.hblitsize; i--;) {
		u32 bltadat, blitahold;
		if (bltadatptr) {
		    bltadat = amigaMemChipGetWord (bltadatptr);
		    bltadatptr += 2;
		} else
		    bltadat = blt_info.bltadat;
		bltadat &= blit_masktable_p[i];
		blitahold = (((u32)preva << 16) | bltadat) >> blt_info.blitashift;
		preva = bltadat;

		if (bltbdatptr) {
			u16 bltbdat = amigaMemChipGetWord (bltbdatptr);
		    bltbdatptr += 2;
			blitbhold = (((u32)prevb << 16) | bltbdat) >> blt_info.blitbshift;
		    prevb = bltbdat;
		}
		if (bltcdatptr) {
		    blt_info.bltcdat = amigaMemChipGetWord (bltcdatptr);
		    bltcdatptr += 2;
		}
		if (dstp) amigaMemChipPutWord (dstp, blt_info.bltddat);
		blt_info.bltddat = blit_func (blitahold, blitbhold, blt_info.bltcdat, mt) & 0xFFFF;
		if (blitfill) {
			u16 d = blt_info.bltddat;
		    int ifemode = blitife ? 2 : 0;
		    int fc1 = blit_filltable[d & 255][ifemode + blitfc][1];
		    blt_info.bltddat = (blit_filltable[d & 255][ifemode + blitfc][0]
					+ (blit_filltable[d >> 8][ifemode + fc1][0] << 8));
		    blitfc = blit_filltable[d >> 8][ifemode + fc1][1];
		}
		if (blt_info.bltddat)
		    blt_info.blitzero = 0;
		if (bltddatptr) {
		    dstp = bltddatptr;
		    bltddatptr += 2;
		}
	    }
	    if (bltadatptr) bltadatptr += blt_info.bltamod;
	    if (bltbdatptr) bltbdatptr += blt_info.bltbmod;
	    if (bltcdatptr) bltcdatptr += blt_info.bltcmod;
	    if (bltddatptr) bltddatptr += blt_info.bltdmod;
	}
	if (dstp) amigaMemChipPutWord (dstp, blt_info.bltddat);
	blt_info.bltbhold = blitbhold;
    }
    blit_masktable[BLITTER_MAX_WORDS - 1] = 0xFFFF;
    blit_masktable[BLITTER_MAX_WORDS - blt_info.hblitsize] = 0xFFFF;

    bltstate = BLT_done;
}

static void blitter_dofast_desc(void)
{
    int i,j;
	u32 bltadatptr = 0, bltbdatptr = 0, bltcdatptr = 0, bltddatptr = 0;
	u8 mt = bltcon0 & 0xFF;

    blit_masktable[BLITTER_MAX_WORDS - 1] = blt_info.bltafwm;
    blit_masktable[BLITTER_MAX_WORDS - blt_info.hblitsize] &= blt_info.bltalwm;

#ifdef DEBUG_BLITTER
    dbgf("blitter_dofast_desc bltafwm=0x%X, bltcon0=0x%X\n",blt_info.bltafwm,bltcon0);
#endif
    if (bltcon0 & 0x800) {
	bltadatptr = bltapt;
	bltapt -= ((blt_info.hblitsize*2) + blt_info.bltamod)*blt_info.vblitsize;
    }
    if (bltcon0 & 0x400) {
	bltbdatptr = bltbpt;
	bltbpt -= ((blt_info.hblitsize*2) + blt_info.bltbmod)*blt_info.vblitsize;
    }
    if (bltcon0 & 0x200) {
	bltcdatptr = bltcpt;
	bltcpt -= ((blt_info.hblitsize*2) + blt_info.bltcmod)*blt_info.vblitsize;
    }
    if (bltcon0 & 0x100) {
	bltddatptr = bltdpt;
	bltdpt -= ((blt_info.hblitsize*2) + blt_info.bltdmod)*blt_info.vblitsize;
    }

#ifdef DEBUG_BLITTER
    print_bltinfo(&blt_info);
#endif

    if (blitfunc_dofast_desc[mt] && !blitfill)
    {
#ifdef DEBUG_BLITTER
	dbgf("blitfunc_dofast_desc[%i](0x%X,0x%X,0x%X,0x%X)\n",mt,bltadatptr, bltbdatptr, bltcdatptr, bltddatptr);
#endif
	(*blitfunc_dofast_desc[mt])(bltadatptr, bltbdatptr, bltcdatptr, bltddatptr, &blt_info);
    }
    else {
	u32 blitbhold = blt_info.bltbhold;
	u32 preva = 0, prevb = 0;
	u32 dstp = 0;
	u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - blt_info.hblitsize;

#ifdef DEBUG_BLITTER
	dbgf("bltbhold=0x%X, vblitsize=0x%X, bltcon1=0x%X\n",blt_info.bltbhold,blt_info.vblitsize,bltcon1);
#endif
/*	if (!blitfill) qDebug ("minterm %x not present\n",mt);*/
	for (j = blt_info.vblitsize; j--;) {
	    blitfc = !!(bltcon1 & 0x4);
	    for (i = blt_info.hblitsize; i--;) {
		u32 bltadat, blitahold;
		if (bltadatptr) {
		    bltadat = amigaMemChipGetWord (bltadatptr);
		    bltadatptr -= 2;
		} else
		    bltadat = blt_info.bltadat;
		bltadat &= blit_masktable_p[i];
		blitahold = (((u32)bltadat << 16) | preva) >> blt_info.blitdownashift;
		preva = bltadat;
		if (bltbdatptr) {
			u16 bltbdat = amigaMemChipGetWord (bltbdatptr);
		    bltbdatptr -= 2;
			blitbhold = (((u32)bltbdat << 16) | prevb) >> blt_info.blitdownbshift;
		    prevb = bltbdat;
		}
		if (bltcdatptr) {
		    blt_info.bltcdat = amigaMemChipGetWord (bltcdatptr);
		    bltcdatptr -= 2;
		}
		if (dstp) amigaMemChipPutWord (dstp, blt_info.bltddat);
		blt_info.bltddat = blit_func (blitahold, blitbhold, blt_info.bltcdat, mt) & 0xFFFF;
		if (blitfill) {
			u16 d = blt_info.bltddat;
		    int ifemode = blitife ? 2 : 0;
		    int fc1 = blit_filltable[d & 255][ifemode + blitfc][1];
		    blt_info.bltddat = (blit_filltable[d & 255][ifemode + blitfc][0]
					+ (blit_filltable[d >> 8][ifemode + fc1][0] << 8));
		    blitfc = blit_filltable[d >> 8][ifemode + fc1][1];
		}
		if (blt_info.bltddat)
		    blt_info.blitzero = 0;
		if (bltddatptr) {
		    dstp = bltddatptr;
		    bltddatptr -= 2;
		}
	    }
	    if (bltadatptr) bltadatptr -= blt_info.bltamod;
	    if (bltbdatptr) bltbdatptr -= blt_info.bltbmod;
	    if (bltcdatptr) bltcdatptr -= blt_info.bltcmod;
	    if (bltddatptr) bltddatptr -= blt_info.bltdmod;
	}
	if (dstp) amigaMemChipPutWord (dstp, blt_info.bltddat);
	blt_info.bltbhold = blitbhold;
    }
    blit_masktable[BLITTER_MAX_WORDS - 1] = 0xFFFF;
    blit_masktable[BLITTER_MAX_WORDS - blt_info.hblitsize] = 0xFFFF;

    bltstate = BLT_done;
}

static __inline__ int blitter_read(void)
{
#ifdef DEBUG_BLITTER
    dbgf("blitter_read -> dmaen=0x%X\n",dmaen(DMA_BLITTER));
#endif
    if (bltcon0 & 0xe00){
	if (!dmaen(DMA_BLITTER))
	    return 1;
	if (bltcon0 & 0x200) blt_info.bltcdat = amigaMemChipBank.wget(bltcpt);
    }
    bltstate = BLT_work;
#ifdef DEBUG_BLITTER
    dbgf("\t ret=0x%X\n",((bltcon0 & 0xE00) != 0));
#endif
    return (bltcon0 & 0xE00) != 0;
}

static __inline__ int blitter_write(void)
{
#ifdef DEBUG_BLITTER
    dbgf("blitter_write -> dmaen=0x%X\n",dmaen(DMA_BLITTER));
#endif
    if (blt_info.bltddat) blt_info.blitzero = 0;
    if ((bltcon0 & 0x100) || blitline){
	if (!dmaen(DMA_BLITTER)) return 1;
	amigaMemChipBank.wput(bltdpt, blt_info.bltddat);
    }
    bltstate = BLT_next;
#ifdef DEBUG_BLITTER
    dbgf("\t ret=0x%X\n",((bltcon0 & 0x100) != 0));
#endif
    return (bltcon0 & 0x100) != 0;
}

static __inline__ void blitter_line_incx(void)
{
    if (++blinea_shift == 16) {
	blinea_shift = 0;
	bltcnxlpt += 2;
	bltdnxlpt += 2;
    }
#ifdef DEBUG_BLITTER
    dbgf("blitter_line_incx -> blinea_shift=0x%X, bltcnxlpt=0x%X, bltdnxlpt=0x%X\n",blinea_shift,bltcnxlpt,bltdnxlpt);
#endif
}

static __inline__ void blitter_line_decx(void)
{
    if (blinea_shift-- == 0) {
	blinea_shift = 15;
	bltcnxlpt -= 2;
	bltdnxlpt -= 2;
    }
#ifdef DEBUG_BLITTER
    dbgf("blitter_line_decx -> blinea_shift=0x%X, bltcnxlpt=0x%X, bltdnxlpt=0x%X\n",blinea_shift,bltcnxlpt,bltdnxlpt);
#endif
}

static __inline__ void blitter_line_decy(void)
{
    bltcnxlpt -= blt_info.bltcmod;
    bltdnxlpt -= blt_info.bltcmod; /* ??? am I wrong or doesn't KS1.3 set bltdmod? */
    blitonedot = 0;
#ifdef DEBUG_BLITTER
    dbgf("blitter_line_decy -> bltcnxlpt=0x%X, bltdnxlpt=0x%X\n",bltcnxlpt,bltdnxlpt);
#endif
}

static __inline__ void blitter_line_incy(void)
{
    bltcnxlpt += blt_info.bltcmod;
    bltdnxlpt += blt_info.bltcmod; /* ??? */
    blitonedot = 0;
#ifdef DEBUG_BLITTER
    dbgf("blitter_line_incy -> bltcnxlpt=0x%X, bltdnxlpt=0x%X\n",bltcnxlpt,bltdnxlpt);
#endif
}

static void blitter_line(void)
{
	u16 blitahold = blinea >> blinea_shift, blitbhold = blineb & 1 ? 0xFFFF : 0, blitchold = blt_info.bltcdat;
    blt_info.bltddat = 0;

#ifdef DEBUG_BLITTER
    dbgf("blitter_line blitahold=0x%X, blinea=0x%X, blinea_shift=0x%X, blineb=0x%X\n",blitahold,blinea,blinea_shift,blineb);
#endif
    if (blitsing && blitonedot) blitahold = 0;
    blitonedot = 1;
    blt_info.bltddat = blit_func(blitahold, blitbhold, blitchold, bltcon0 & 0xFF);
    if (!blitsign){
	bltapt += (s16)blt_info.bltamod;
	if (bltcon1 & 0x10){
	    if (bltcon1 & 0x8)
		blitter_line_decy();
	    else
		blitter_line_incy();
	} else {
	    if (bltcon1 & 0x8)
		blitter_line_decx();
	    else
		blitter_line_incx();
	}
    } else {
	bltapt += (s16)blt_info.bltbmod;
    }
    if (bltcon1 & 0x10){
	if (bltcon1 & 0x4)
	    blitter_line_decx();
	else
	    blitter_line_incx();
    } else {
	if (bltcon1 & 0x4)
	    blitter_line_decy();
	else
	    blitter_line_incy();
    }
	blitsign = 0 > (s16)bltapt;
    bltstate = BLT_write;
}

static __inline__ void blitter_nxline(void)
{
    bltcpt = bltcnxlpt;
    bltdpt = bltdnxlpt;
    blineb = (blineb << 1) | (blineb >> 15);
    if (--blt_info.vblitsize == 0) {
	bltstate = BLT_done;
    } else {
	bltstate = BLT_read;
    }
#ifdef DEBUG_BLITTER
    dbgf("blitter_nxline -> bltcpt=0x%X, bltdpt=0x%X, blineb=0x%X\n",bltcpt,bltdpt,blineb);
#endif
}

static void blit_init(void)
{
    blitlpos = 0;
    blt_info.blitzero = 1;
    blitline = bltcon1 & 1;
    blt_info.blitashift = bltcon0 >> 12;
    blt_info.blitdownashift = 16 - blt_info.blitashift;
    blt_info.blitbshift = bltcon1 >> 12;
    blt_info.blitdownbshift = 16 - blt_info.blitbshift;

    if (blitline) {
#ifdef DEBUG_BLITTER
	if (blt_info.hblitsize != 2)
	    qDebug ("weird hblitsize in linemode: %d\n", blt_info.hblitsize);
#endif

	bltcnxlpt = bltcpt;
	bltdnxlpt = bltdpt;
	blitsing = bltcon1 & 0x2;
	blinea = blt_info.bltadat;
	blineb = (blt_info.bltbdat >> blt_info.blitbshift) | (blt_info.bltbdat << (16-blt_info.blitbshift));
#if 0
	if (blineb != 0xFFFF && blineb != 0)
	    qDebug ("%x %x %d %x\n", blineb, blt_info.bltbdat, blt_info.blitbshift, bltcon1);
#endif
	blitsign = bltcon1 & 0x40;
	blitonedot = 0;
#ifdef DEBUG_BLITTER
	dbgf("blit_init blinea=0x%X, blineb=0x%X, bltcnxlpt=0x%X, bltdnxlpt=0x%X,blitsing=0x%X\n",blinea,blineb,bltcnxlpt,bltdnxlpt,blitsing);
#endif
    } else {
	blitfc = !!(bltcon1 & 0x4);
	blitife = bltcon1 & 0x8;
	blitfill = bltcon1 & 0x18;
	if ((bltcon1 & 0x18) == 0x18) {
	    /* Digital "Trash" demo does this; others too. Apparently, no
	     * negative effects. */
#ifdef DEBUG_BLITTER
	    static int warn = 1;
	    if (warn)
		qDebug ("warning: weird fill mode (further messages suppressed)\n");
	    warn = 0;
#endif
	}
	blitdesc = bltcon1 & 0x2;
#ifdef DEBUG_BLITTER
	if (blitfill && !blitdesc) {
	    static int warn = 1;
	    if (warn)
		qDebug ("warning: blitter fill without desc (further messages suppressed)\n");
	    warn = 0;
	}

	dbgf("blit_init blitfc=0x%X, blitife=0x%X, blitfill=0x%X, blitdesc=0x%X\n",blitfc,blitife,blitfill,blitdesc);
#endif
    }
}

static void actually_do_blit(void)
{
#ifdef DEBUG_BLITTER
    dbgf("actually_do_blit -> blitline=0x%X\n",blitline);
#endif
    if (blitline) {
	do {
	    blitter_read();
	    blitter_line();
	    blitter_write();
	    blitter_nxline();
	} while (bltstate != BLT_done);
    } else {
	/*blitcount[bltcon0 & 0xff]++;  blitter debug */
	if (blitdesc) blitter_dofast_desc();
	else blitter_dofast();
    }
    blitterDoneNotify ();
}


void amigaBlitterHandler(void)
{
    if (!dmaen(DMA_BLITTER)) {
		amigaEvents[AmigaEventBlitter].active = 1;
		amigaEvents[AmigaEventBlitter].time = 10 * CYCLE_UNIT + amigaCycles; /* wait a little */
		return; /* gotta come back later. */
    }
    actually_do_blit();

    INTREQ(0x8040);

    amigaEvents[AmigaEventBlitter].active = 0;
    amigaCpuClearSpcFlag (SpcFlagBltNasty);
}

static u8 blit_cycle_diagram_start[][10] =
{
    { 0, 1, 0 },		/* 0 */
    { 0, 2, 4,0 },		/* 1 */
    { 0, 2, 3,0 },		/* 2 */
    { 2, 3, 3,0, 0,3,4 },	/* 3 */
    { 0, 3, 2,0,0 },		/* 4 */
    { 2, 3, 2,0, 0,2,4 },	/* 5 */
    { 0, 3, 2,3,0 },		/* 6 */
    { 3, 4, 2,3,0, 0,2,3,4 },	/* 7 */
    { 0, 2, 1,0 },		/* 8 */
    { 2, 2, 1,0, 1,4 },		/* 9 */
    { 0, 2, 1,3 },		/* A */
    { 3, 3, 1,3,0, 1,3,4 },	/* B */
    { 2, 3, 1,2, 0,1,2 },	/* C */
    { 3, 3, 1,2,0, 1,2,4 },	/* D */
    { 0, 3, 1,2,3 },		/* E */
    { 4, 4, 1,2,3,0, 1,2,3,4 }	/* F */
};

static long blit_firstline_cycles;
static long blit_first_cycle;
static int blit_last_cycle;
static u8 *blit_diag;

void do_blitter(void) {
    int ch = (bltcon0 & 0x0f00) >> 8;
    blit_diag = blit_cycle_diagram_start[ch];

	blit_firstline_cycles = blit_first_cycle = amigaCycles;
    blit_last_cycle = 0;
    blit_init();

    amigaEvents[AmigaEventBlitter].active = 1;
	amigaEvents[AmigaEventBlitter].time = CYCLE_UNIT + amigaCycles;
    amigaEventsSchedule();

    if (dmaen(DMA_BLITPRI))
		setNasty();
    else
		amigaCpuClearSpcFlag(SpcFlagBltNasty);
}

void maybe_blit(int modulo) {
    if (bltstate == BLT_done)
		return;
	if (modulo && amigaCycles < blit_firstline_cycles)
		return;
	amigaBlitterHandler();
}

int blitnasty() {
    int cycles, ccnt;
    if (!(amigaCpuSpcFlags & SpcFlagBltNasty))
		return 0;
    if (bltstate == BLT_done)
		return 0;
    if (!dmaen(DMA_BLITTER))
		return 0;
	cycles = (amigaCycles - blit_first_cycle) / CYCLE_UNIT;
    ccnt = 0;
    while (blit_last_cycle < cycles) {
		int c;
		if (blit_last_cycle < blit_diag[0])
			c = blit_diag[blit_last_cycle + 2];
		else
			c = blit_diag[((blit_last_cycle - blit_diag[0]) % blit_diag[1]) + 2 + blit_diag[0]];
		blit_last_cycle ++;
		if (!c)
			return 0;
		ccnt++;
    }
    return ccnt;
}
