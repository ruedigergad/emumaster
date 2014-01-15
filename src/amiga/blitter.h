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

#ifndef AMIGABLITTER_H
#define AMIGABLITTER_H

#include <base/emu.h>

struct bltinfo {
    int blitzero;
    int blitashift,blitbshift,blitdownashift,blitdownbshift;
    u16 bltadat, bltbdat, bltcdat,bltddat,bltahold,bltbhold,bltafwm,bltalwm;
    int vblitsize,hblitsize;
    int bltamod,bltbmod,bltcmod,bltdmod;
};

extern enum blitter_states {
    BLT_done, BLT_init, BLT_read, BLT_work, BLT_write, BLT_next
} bltstate;

extern struct bltinfo blt_info;

extern u16 bltsize;
extern u16 bltcon0,bltcon1;
extern int blinea_shift;
extern u32 bltapt,bltbpt,bltcpt,bltdpt;

extern void maybe_blit(int);
extern int blitnasty();
extern void amigaBlitterHandler();
extern void build_blitfilltable();
extern void do_blitter();
extern void blitterDoneNotify (void);
typedef void blitter_func(u32, u32, u32, u32, struct bltinfo *_GCCRES_);

#define BLITTER_MAX_WORDS 2048

extern blitter_func *blitfunc_dofast[256];
extern blitter_func *blitfunc_dofast_desc[256];
extern u32 blit_masktable[BLITTER_MAX_WORDS];

#endif // AMIGABLITTER_H
