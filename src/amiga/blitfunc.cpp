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

#include "amiga.h"
#include "custom.h"
#include "mem.h"
#include "blitter.h"
#include "blitfunc.h"

/*
	gno: optimized..
	notaz: too :)
*/

void blitdofast_0 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
unsigned int i,j,hblitsize,bltdmod;
if (!ptd || !b->vblitsize || !b->hblitsize) return;
hblitsize = b->hblitsize;
bltdmod = b->bltdmod;
j = b->vblitsize;
do {
	i = hblitsize;
	do {
		amigaMemChipPutWord (ptd, 0);
		ptd += 2;
	} while (--i);
	ptd += bltdmod;
} while (--j);
}
void blitdofast_desc_0 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
unsigned int i,j,hblitsize,bltdmod;
if (!ptd || !b->vblitsize || !b->hblitsize) return;
hblitsize = b->hblitsize;
bltdmod = b->bltdmod;
j = b->vblitsize;
do {
	i = hblitsize;
	do {
		amigaMemChipPutWord (ptd, 0);
		ptd -= 2;
	} while (--i);
	ptd -= bltdmod;
} while (--j);
}
void blitdofast_a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 srcc = b->bltcdat;
u32 hblitsize = b->hblitsize;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - hblitsize;
if (!b->vblitsize || !hblitsize) return;
for (j = b->vblitsize; j--;) {
	for (i = hblitsize; i--;) {
		u32 bltadat, srca, dstd;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((~srca & srcc));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 hblitsize = b->hblitsize;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((~srca & srcc));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_2a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc & ~(srca & srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_2a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc & ~(srca & srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_30 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srca & ~srcb));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_30 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srca & ~srcb));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_3a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcb ^ (srca | (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_3a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcb ^ (srca | (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_3c (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srca ^ srcb));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_3c (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srca ^ srcb));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_4a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & (srcb | srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_4a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & (srcb | srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_6a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_6a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_8a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc & (~srca | srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_8a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc & (~srca | srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_8c (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcb & (~srca | srcc)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_8c (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcb & (~srca | srcc)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_9a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & ~srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_9a (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & ~srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_a8 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc & (srca | srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_a8 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc & (srca | srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_aa (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 srcc = b->bltcdat;
u32 dstd=0;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		dstd = (srcc);
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_aa (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 srcc = b->bltcdat;
u32 dstd=0;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		dstd = (srcc);
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_b1 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = (~(srca ^ (srcc | (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_b1 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = (~(srca ^ (srcc | (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_ca (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_ca (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc ^ (srca & (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_cc (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		dstd = (srcb);
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_cc (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		dstd = (srcb);
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_d8 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srca ^ (srcc & (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_d8 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srca ^ (srcc & (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_e2 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc ^ (srcb & (srca ^ srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_e2 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc ^ (srcb & (srca ^ srcc))));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_ea (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srcc | (srca & srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_ea (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srcc | (srca & srcb)));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_f0 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = (srca);
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptd) ptd += b->bltdmod;
}
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_f0 (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = (srca);
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptd) ptd -= b->bltdmod;
}
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_fa (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc += 2; }
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srca | srcc));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltcdat = srcc;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_fa (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 srcc = b->bltcdat;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptc) { srcc = amigaMemChipGetWord (ptc); ptc -= 2; }
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srca | srcc));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltcdat = srcc;
}
void blitdofast_fc (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
register unsigned int i,j;
u32 totald = 0;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;

		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb += 2;
			srcb = (((u32)prevb << 16) | bltbdat) >> b->blitbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta += 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)preva << 16) | bltadat) >> b->blitashift;
		preva = bltadat;
		dstd = ((srca | srcb));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_fc (u32 pta, u32 ptb, u32 ptc, u32 ptd, struct bltinfo *_GCCRES_ b)
{
u32 totald = 0;
register unsigned int i,j;
u32 preva = 0;
u32 prevb = 0, srcb = b->bltbhold;
u32 dstd=0;
u32 *blit_masktable_p = blit_masktable + BLITTER_MAX_WORDS - b->hblitsize;
for (j = b->vblitsize; j--;) {
	for (i = b->hblitsize; i--;) {
		u32 bltadat, srca;
		if (ptb) {
			u32 bltbdat = amigaMemChipGetWord (ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | prevb) >> b->blitdownbshift;
			prevb = bltbdat;
		}
		if (pta) { bltadat = amigaMemChipGetWord (pta); pta -= 2; } else { bltadat = blt_info.bltadat; }
		bltadat &= blit_masktable_p[i];
		srca = (((u32)bltadat << 16) | preva) >> b->blitdownashift;
		preva = bltadat;
		dstd = ((srca | srcb));
		totald |= dstd;
		if (ptd) { amigaMemChipPutWord (ptd, dstd); ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if ((totald<<16) != 0) b->blitzero = 0;
}
