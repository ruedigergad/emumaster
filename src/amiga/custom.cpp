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
#include "events.h"
#include "mem.h"
#include "custom.h"
#include "cpu.h"
#include "cia.h"
#include "disk.h"
#include "blitter.h"
#include "spu.h"
#include "drawing.h"
#include <stdlib.h>

static inline void setNasty() {
	amigaCpuReleaseTimeslice();
	amigaCpuSetSpcFlag(SpcFlagBltNasty);
}

static int fm_maxplane_prepared;

static const int fetch_prepared[72] = {
	1,0,0,0,0,0,0,0, // 0
	1,0,0,0,0,0,0,0, // 1
	1,0,0,0,0,0,0,0, // 2
	3,1,2,0,0,0,0,0, // 3
	3,1,2,0,0,0,0,0, // 4
	3,1,2,0,0,0,0,0, // 5
	7,3,5,1,6,2,4,0, // 6
	7,3,5,1,6,2,4,0, // 7
	7,3,5,1,6,2,4,0  // 8
};

static u16 last_custom_value;

/* Mouse and joystick emulation */

int  amigaInputPortButtons[2]	= { 0, 0 };
uint amigaInputPortDir[2]		= { 0, 0 };
static u16 potgo;

/* Events */

u32 amigaCycles;
u32 amigaNextEvent;
AmigaEvent amigaEvents[AmigaEventMax];
bool amigaEventVSync = false;

static u32 lastHSyncCycles = 0;
static int vpos;
static u16 lof;
// TODO maybe next_lineno->vpos
static int next_lineno;

static u32 sprtaba[256];
static u32 sprtabb[256];

/*
 * Hardware registers of all sorts.
 */

static int vsync_handler_cnt_disk_change = 0;

u16 intena;
u16 intreq;
u16 dmacon;
u16 adkcon; /* used by audio code */

static u32 cop1lc;
static u32 cop2lc;
static u32 copcon;
 
int maxhpos = MAXHPOS_PAL;
int maxvpos = MAXVPOS_PAL;
int minfirstline = MINFIRSTLINE_PAL;
int vblank_hz = VBLANK_HZ_PAL;

static const int MaxSprites = 8;

class AmigaSprite {
public:
	/* This is but an educated guess. It seems to be correct, but this stuff
	 * isn't documented well. */
	enum State { Restart, WaitingStart, WaitingStop };

	u32 pt;
	int xpos;
	int vstart;
	int vstop;
	int armed;
	State state;
};

static AmigaSprite spr[MaxSprites];

static const int SpriteVblankEndline = 25;

static u32 sprpos[MaxSprites];
static u32 sprctl[MaxSprites];
static u16 sprdata[MaxSprites][4];
static u16 sprdatb[MaxSprites][4];

static int last_sprite_point;
static int sprite_width, sprres, sprite_buffer_res;

static u32 bpl1dat;
static s16 bpl1mod;
static s16 bpl2mod;

static u32 bplpt[8];

static struct color_entry currentColors;
static uint bplcon0, bplcon1, bplcon2, bplcon3, bplcon4;
static uint bplcon0Planes, bplcon0Resolution;
static uint diwstrt, diwstop;
static uint ddfstrt, ddfstop;

/* The display and data fetch windows */

enum diw_states { DIW_waiting_start, DIW_waiting_stop };

static int plffirstline, plflastline;
static int plfstrt, plfstop;
static int last_diw_pix_hpos, last_ddf_pix_hpos, last_decide_line_hpos;
static int last_fetch_hpos, last_sprite_hpos;
int diwfirstword, diwlastword;
static enum diw_states diwstate, hdiwstate;

enum copper_states {
	COP_stop,
	COP_read1_in2,
	COP_read1_wr_in4,
	COP_read1_wr_in2,
	COP_read1,
	COP_read2_wr_in2,
	COP_read2,
	COP_bltwait,
	COP_wait_in4,
	COP_wait_in2,
	COP_skip_in4,
	COP_skip_in2,
	COP_wait1,
	COP_wait
};

struct copper {
	/* The current instruction words.  */
	uint i1, i2;
	uint saved_i1, saved_i2;
	enum copper_states state;
	/* Instruction pointer.  */
	u32 ip;
	int hpos, vpos;
	uint ignore_next;
	int vcmp, hcmp;

	/* When we schedule a copper event, knowing a few things about the future
	   of the copper list can reduce the number of sync_with_cpu calls
	   dramatically.  */
	uint first_sync;
	uint regtypes_modified;
};

#define REGTYPE_NONE 0
#define REGTYPE_COLOR 1
#define REGTYPE_SPRITE 2
#define REGTYPE_PLANE 4
#define REGTYPE_BLITTER 8
#define REGTYPE_JOYPORT 16
#define REGTYPE_DISK 32
#define REGTYPE_POS 64
#define REGTYPE_AUDIO 128

#define REGTYPE_ALL 255
/* Always set in regtypes_modified, to enable a forced update when things like
   DMACON, BPLCON0, COPJMPx get written.  */
#define REGTYPE_FORCE 256


static uint regtypes[512];

static struct copper cop_state;
static int coppperEnabledCurrLine;

#define NR_COPPER_RECORDS 1

/* Record copper activity for the debugger.  */
struct cop_record {
	int hpos;
	int vpos;
	u32 addr;
};

/* Recording of custom chip register changes.  */
static int current_change_set = 0;

struct sprite_entry  *sprite_entries[2];
struct color_change *color_changes[2];
//#define MAX_SPRITE_ENTRY (1024*8)
//#define MAX_COLOR_CHANGE (1024*16)
#define MAX_SPRITE_ENTRY (1024*6)
#define MAX_COLOR_CHANGE (1024*10)

struct decision line_decisions[2 * (MAXVPOS + 1) + 1];
struct color_entry color_tables[2][(MAXVPOS + 1) * 2];

static int next_sprite_entry = 0;
static int prev_next_sprite_entry;
static int next_sprite_forced = 1;

struct sprite_entry *curr_sprite_entries, *prev_sprite_entries;
struct color_change *curr_color_changes;
struct draw_info curr_drawinfo[2 * (MAXVPOS + 1) + 1];
struct color_entry *curr_color_tables;

static int next_color_change;
static int next_color_entry, remembered_color_entry;

static struct decision currLineDecision;
static int passed_plfstop, fetch_cycle;

enum fetchstate {
	fetch_not_started,
	fetch_started,
	fetch_was_plane0
} fetch_state;

static void custom_wput_1(int, u32, u32);

/*
 * helper functions
 */
static __inline__ void setclr(u16 *_GCCRES_ p, u16 val) {
	if (val & 0x8000)
		*p |= val & 0x7FFF;
	else
		*p &= ~val;
}

static inline uint currentHPos()
{ return (amigaCycles - lastHSyncCycles) / CYCLE_UNIT; }

static void do_sprites (int currhp);

static inline void remember_ctable() {
	if (currLineDecision.ctable == -1) {
		if (remembered_color_entry == -1) {
			/* The colors changed since we last recorded a color map. Record a
			 * new one. */
			color_reg_cpy(curr_color_tables + next_color_entry, &currentColors);
			remembered_color_entry = next_color_entry++;
		}
		currLineDecision.ctable = remembered_color_entry;
	}
}


/* Called to determine the state of the horizontal display window state
 * machine at the current position. It might have changed since we last
 * checked.  */
static void decide_diw(int hpos) {
	int pix_hpos = coord_diw_to_window_x (hpos << 1);
	if (hdiwstate == DIW_waiting_start && currLineDecision.diwfirstword == -1
	&& pix_hpos >= diwfirstword && last_diw_pix_hpos < diwfirstword) {
		currLineDecision.diwfirstword = diwfirstword < 0 ? 0 : diwfirstword;
		hdiwstate = DIW_waiting_stop;
		currLineDecision.diwlastword = -1;
	}
	if (hdiwstate == DIW_waiting_stop && currLineDecision.diwlastword == -1
	&& pix_hpos >= diwlastword && last_diw_pix_hpos < diwlastword) {
		currLineDecision.diwlastword = diwlastword < 0 ? 0 : diwlastword;
		hdiwstate = DIW_waiting_start;
	}
	last_diw_pix_hpos = pix_hpos;
}

/* The HRM says 0xD8, but that can't work... */
#define HARD_DDF_STOP (0xD4)


static inline void finish_playfield_line() {
	switch (bplcon0Planes) {
	case 8: bplpt[7] += bpl2mod;
	case 7: bplpt[6] += bpl1mod;
	case 6: bplpt[5] += bpl2mod;
	case 5: bplpt[4] += bpl1mod;
	case 4: bplpt[3] += bpl2mod;
	case 3: bplpt[2] += bpl1mod;
	case 2: bplpt[1] += bpl2mod;
	case 1: bplpt[0] += bpl1mod;
	}
	currLineDecision.bplcon0 = bplcon0;
	currLineDecision.bplcon2 = bplcon2;
	currLineDecision.bplcon3 = bplcon3;
	currLineDecision.bplcon4 = bplcon4;
}

/* The fetch unit mainly controls ddf stop.  It's the number of cycles that
   are contained in an indivisible block during which ddf is active.  E.g.
   if DDF starts at 0x30, and fetchunit is 8, then possible DDF stops are
   0x30 + n * 8.  */
#define FETCHUNIT 8
#define FETCHUNIT_MASK 7
// static int fetchunit, fetchunit_mask;
/* The delay before fetching the same bitplane ajain.  Can be larger than
   the number of bitplanes; in that case there are additional empty cycles
   with no data fetch (this happens for high fetchmodes and low
   resolutions).  */
static int fetchstart, fetchstart_mask;
/* fm_maxplane holds the maximum number of planes possible with the current
   fetch mode.  This selects the cycle diagram:
   8 planes: 73516240
   4 planes: 3120
   2 planes: 10.  */
static int fm_maxplane;

/* The corresponding values, by fetchmode and display resolution.  */
static const int fetchstarts[] = { 3,2,1,0, 4,3,2,0, 5,4,3,0 };
static const int fm_maxplanes[] = { 3,2,1,0, 3,3,2,0, 3,3,3,0 };

static int cycle_diagram_table[3][9][32];
static int *curr_diagram;
static const int cycle_sequences[3*8] = { 2,1,2,1,2,1,2,1, 4,2,3,1,4,2,3,1, 8,4,6,2,7,3,5,1 };

static void create_cycle_diagram_table() {
	for (int res = 0; res <= 2; res++) {
		int max_planes = fm_maxplanes[res];
		int fetch_start = 1 << fetchstarts[res];
		const int *cycle_sequence = &cycle_sequences[(max_planes - 1) << 3];
		max_planes = 1 << max_planes;
		for (int planes = 0; planes <= 8; planes++) {
			for (int cycle = 0; cycle < 32; cycle++)
				cycle_diagram_table[res][planes][cycle] = -1;
			if (planes <= max_planes) {
				for (int cycle = 0; cycle < fetch_start; cycle++) {
					bool b = (cycle < max_planes && planes >= cycle_sequence[cycle & 7]);
					cycle_diagram_table[res][planes][cycle] = (b ? 1 : 0);
				}
			}
		}
	}
}

/* Used by the copper.  */
static int estimated_last_fetch_cycle;
static int cycle_diagram_shift;

static void estimate_last_fetch_cycle(int hpos) {
	if (! passed_plfstop) {
		int stop = plfstop < hpos || plfstop > HARD_DDF_STOP ? HARD_DDF_STOP : plfstop;
		/* We know that fetching is up-to-date up until hpos, so we can use fetch_cycle.  */
		int fetch_cycle_at_stop = fetch_cycle + (stop - hpos);
		int starting_last_block_at = (fetch_cycle_at_stop + FETCHUNIT - 1) & ~(FETCHUNIT - 1);

		estimated_last_fetch_cycle = hpos + (starting_last_block_at - fetch_cycle) + FETCHUNIT;
	} else {
		int starting_last_block_at = (fetch_cycle + FETCHUNIT - 1) & ~(FETCHUNIT - 1);
		if (passed_plfstop == 2)
			starting_last_block_at -= FETCHUNIT;

		estimated_last_fetch_cycle = hpos + (starting_last_block_at - fetch_cycle) + FETCHUNIT;
	}
}

static int out_nbits, out_offs;
static u32 outword[MAX_PLANES];
static u32 todisplay[MAX_PLANES];
static u32 fetched[MAX_PLANES];

/* Expansions from bplcon0/bplcon1.  */
static int toscr_res, toscr_res2, toscr_res15, toscr_delay[2], toscr_nr_planes;

/* The number of bits left from the last fetched words.  
   This is an optimization - conceptually, we have to make sure the result is
   the same as if toscr is called in each clock cycle.  However, to speed this
   up, we accumulate display data; this variable keeps track of how much. 
   Thus, once we do call toscr_nbits (which happens at least every 16 bits),
   we can do more work at once.  */
static int toscr_nbits;

static int delayoffset;

static const int maxplanes_ocs[]={ 6,4,0,0 };

static inline void compute_delay_offset(int hpos) {
	delayoffset = ((hpos - fm_maxplane - 0x18) & fetchstart_mask) << 1;
	delayoffset &= ~7;
	if (delayoffset & 8)
		delayoffset = 8;
	else if (delayoffset & 16)
		delayoffset = 16;
	else if (delayoffset & 32)
		delayoffset = 32;
	else
		delayoffset = 0;
}

static void expand_fmodes() {
	int res = bplcon0Resolution;
	int fetchstart_shift = fetchstarts[res];
	fetchstart = 1 << fetchstart_shift;
	fetchstart_mask = fetchstart - 1;
	int fm_maxplane_shift = fm_maxplanes[res];
	fm_maxplane = 1 << fm_maxplane_shift;
	fm_maxplane_prepared = fm_maxplane << 3;
}

/* Expand bplcon0/bplcon1 into the toscr_xxx variables.  */
static inline void compute_toscr_delay_1() {
	int delay1 = (bplcon1 & 0x0f) | ((bplcon1 & 0x0c00) >> 6);
	int delay2 = ((bplcon1 >> 4) & 0x0f) | (((bplcon1 >> 4) & 0x0c00) >> 6);
	delay1 += delayoffset;
	delay2 += delayoffset;
	toscr_delay[0] = (delay1 & toscr_res15) << toscr_res;
	toscr_delay[1] = (delay2 & toscr_res15) << toscr_res;
}

static inline void compute_toscr_delay_0() {
	toscr_res = bplcon0Resolution;
	toscr_res2 = 2 << toscr_res;
	toscr_res15 = 15 >> toscr_res;
	toscr_nr_planes = bplcon0Planes;
	if (toscr_nr_planes > maxplanes_ocs[toscr_res]) {
		int v = bplcon0;
		v &= ~0x7010;
		toscr_res = GET_RES(v);
		toscr_nr_planes = GET_PLANES(v);
	}
	compute_toscr_delay_1();
}

static inline void maybe_first_bpl1dat(uint hpos) {
	if (currLineDecision.plfleft == -1) {
		currLineDecision.plfleft = hpos;
		compute_delay_offset(hpos);
		compute_toscr_delay_1();
	}
}

static inline void update_toscr_planes() {
	if (toscr_nr_planes > currLineDecision.nr_planes)
		currLineDecision.nr_planes = toscr_nr_planes;
}

static inline void toscr_1(int nbits) {
	int delay1 = toscr_delay[0];
	int delay2 = toscr_delay[1];
	u32 mask = 0xFFFF >> (16 - nbits);
	uint j = toscr_nr_planes;
	for (uint i = 0; i < j; i += 2) {
		outword[i] <<= nbits;
		outword[i] |= (todisplay[i] >> (16 - nbits + delay1)) & mask;
		todisplay[i] <<= nbits;
	}
	for (uint i = 1; i < j; i += 2) {
		outword[i] <<= nbits;
		outword[i] |= (todisplay[i] >> (16 - nbits + delay2)) & mask;
		todisplay[i] <<= nbits;
	}

	out_nbits += nbits;
	if (out_nbits == 32) {
		u8 *dataptr = line_data[next_lineno] + (out_offs << 2);
		uint j = currLineDecision.nr_planes;
		for (uint i = 0; i < j; i++) {
			register u32 *dataptr32 = (u32 *)dataptr;
			if (*dataptr32 != outword[i])
				*dataptr32 = outword[i];
			dataptr += MAX_WORDS_PER_LINE * 2;
		}
		out_offs++;
		out_nbits = 0;
	}
}

static inline void toscr(int nbits) {
	int t = 32 - out_nbits;
	if (nbits > 16) {
		int nn=16;
		if (t < nn) {
			toscr_1(t);
			nn -= t;
		}
		toscr_1(nn);
		nbits -= 16;
	}
	if (t < nbits) {
		toscr_1(t);
		nbits -= t;
	}
	toscr_1(nbits);
}

static inline void flush_display() {
	if (toscr_nbits > 0 && currLineDecision.plfleft != -1)
		toscr(toscr_nbits);
	toscr_nbits = 0;
} 

/* Called when all planes have been fetched, i.e. when a new block
   of data is available to be displayed.  The data in fetched[] is
   moved into todisplay[].  */
static void beginning_of_plane_block(int pos) {
	flush_display();
	todisplay[0] |= fetched[0];
	todisplay[1] |= fetched[1];
	todisplay[2] |= fetched[2];
	todisplay[3] |= fetched[3];
	todisplay[4] |= fetched[4];
	todisplay[5] |= fetched[5];
	maybe_first_bpl1dat(pos);
}

static inline void long_fetch_ecs(int plane, int nwords, int weird_number_of_bits) {
	u16 *real_pt = (u16 *)amigaMemChipXlate(bplpt[plane], nwords << 1);
	int delay = toscr_delay[(plane & 1)];
	int tmp_nbits = out_nbits;
	u32 shiftbuffer = todisplay[plane];
	u32 outval = outword[plane];
	u32 fetchval = fetched[plane];
	u32 *dataptr = (u32 *)(line_data[next_lineno] + 2 * plane * MAX_WORDS_PER_LINE + 4 * out_offs);
	bplpt[plane] += nwords << 1;
	if (real_pt == 0)
		return;
	while (nwords > 0) {
		int bits_left = 32 - tmp_nbits;
		u32 t;
		shiftbuffer |= fetchval;
		t = (shiftbuffer >> delay) & 0xFFFF;
		if (weird_number_of_bits && bits_left < 16) {
			outval <<= bits_left;
			outval |= t >> (16 - bits_left);
			*dataptr++ = outval;
			outval = t;
			tmp_nbits = 16 - bits_left;
			shiftbuffer <<= 16;
		} else {
			outval = (outval << 16) | t;
			shiftbuffer <<= 16;
			tmp_nbits += 16;
			if (tmp_nbits == 32) {
				*dataptr++ = outval;
				tmp_nbits = 0;
			}
		}
		nwords--;
		fetchval = *real_pt;
		real_pt++;
	}
	fetched[plane] = fetchval;
	todisplay[plane] = shiftbuffer;
	outword[plane] = outval;
}

static void long_fetch_ecs0(int nwords) {
	int max_plane = toscr_nr_planes;
	for (int plane = 0; plane < max_plane; plane++)
		long_fetch_ecs(plane, nwords, 0);
}

static void long_fetch_ecs1(int nwords) {
	int max_plane = toscr_nr_planes;
	for (int plane = 0; plane < max_plane; plane++)
		long_fetch_ecs(plane, nwords, 1);
}

static void do_long_fetch(int nwords) {
	flush_display ();
	if (out_nbits & 15)
		long_fetch_ecs0(nwords);
	else
		long_fetch_ecs1(nwords);
	out_nbits += nwords * 16;
	out_offs += out_nbits >> 5;
	out_nbits &= 31;

	if (toscr_nr_planes > 0)
	fetch_state = fetch_was_plane0;
}


/* make sure fetch that goes beyond maxhpos is finished */
static void finish_final_fetch(int i) {
	passed_plfstop = 3;

	if (currLineDecision.plfleft != -1) {
		int j = 0;
		if (out_nbits <= 16) {
			j += 16;
			toscr_1 (16);
		}
		if (out_nbits != 0) {
			j += 32 - out_nbits;
			toscr_1 (32 - out_nbits);
		}
		j += 32;
		toscr_1(16);
		toscr_1(16);
		currLineDecision.plfright = i+(j >> (1 + toscr_res));
		currLineDecision.plflinelen = out_offs;
		currLineDecision.bplres = toscr_res;
		finish_playfield_line();
	}
}

static inline void fetch(int nr) {
	if (nr < toscr_nr_planes) {
		fetched[nr] = amigaMemChipGetWord(bplpt[nr]);
		bplpt[nr] += 2;
		if (!nr)
			fetch_state = fetch_was_plane0;
	}
}

static inline void FETCH_PREPARED(int fetchCycle)
{ fetch(fetch_prepared[(fetchCycle&fetchstart_mask)|(fm_maxplane_prepared)]); }

static inline bool ONE_FETCH(int POS, int DDFSTOP) {
	if (!passed_plfstop && POS == DDFSTOP)
		passed_plfstop = 1;
	if (!(fetch_cycle & FETCHUNIT_MASK)) {
		if (passed_plfstop == 2) {
			finish_final_fetch (POS);
			return true;
		}
		if (passed_plfstop)
			passed_plfstop++;
	}
	FETCH_PREPARED(fetch_cycle);
	fetch_cycle++;
	toscr_nbits += toscr_res2;
	if (toscr_nbits == 16)
		flush_display();
	return false;
}

static void update_fetch(int until) {
	if (!amigaDrawEnabled || passed_plfstop == 3)
		return;

	/* We need an explicit test ajainst HARD_DDF_STOP here to guard ajainst
	   programs that move the DDFSTOP before our current position before we
	   reach it.  */

	int ddfstop_to_test = HARD_DDF_STOP;
	if (ddfstop >= last_fetch_hpos && ddfstop < HARD_DDF_STOP)
		ddfstop_to_test = ddfstop;

	compute_toscr_delay_0();
	update_toscr_planes();

	int pos = last_fetch_hpos;
	cycle_diagram_shift = (last_fetch_hpos - fetch_cycle) & fetchstart_mask;

	/* First, a loop that prepares us for the speedup code.  We want to enter
	   the SPEEDUP case with fetch_state == fetch_was_plane0, and then unroll
	   whole blocks, so that we end on the same fetch_state ajain.  */
	for (; ; pos++) {
		if (pos == until) {
			if (until >= maxhpos && passed_plfstop == 2)
				finish_final_fetch(pos);
			else
				flush_display();
			return;
		}

		if (fetch_state == fetch_was_plane0)
			break;

		fetch_state = fetch_started;
		if (ONE_FETCH(pos,ddfstop_to_test))
			return;
	}

	/* Unrolled version of the for loop below.  */
	if (! passed_plfstop
	&& (fetch_cycle & fetchstart_mask) == (fm_maxplane & fetchstart_mask)
	&& toscr_nr_planes == currLineDecision.nr_planes) {
		int offs = (pos - fetch_cycle) & FETCHUNIT_MASK;
		int ddf2 = ((ddfstop_to_test - offs + FETCHUNIT - 1) & ~FETCHUNIT_MASK) + offs;
		int ddf3 = ddf2 + FETCHUNIT;
		int stop = until < ddf2 ? until : until < ddf3 ? ddf2 : ddf3;
		int count;

		count = stop - pos;

		if (count >= fetchstart) {
			count &= ~fetchstart_mask;

			if (currLineDecision.plfleft == -1) {
				compute_delay_offset(pos);
				compute_toscr_delay_1();
			}
			do_long_fetch(count >> (3 - toscr_res));

			maybe_first_bpl1dat(pos);

			if (pos <= ddfstop_to_test && pos + count > ddfstop_to_test)
				passed_plfstop = 1;
			if (pos <= ddfstop_to_test && pos + count > ddf2)
				passed_plfstop = 2;
			pos += count;
			fetch_cycle += count;
		}
	}

	for (; pos < until; pos++) {
		if (fetch_state == fetch_was_plane0)
			beginning_of_plane_block(pos);
		fetch_state = fetch_started;
		if (ONE_FETCH(pos,ddfstop_to_test))
			return;
	}
	if (until >= maxhpos && passed_plfstop == 2) {
		finish_final_fetch(pos);
		return;
	}
	flush_display();
}

static inline void decide_fetch(int hpos) {
	if (fetch_state != fetch_not_started && hpos > last_fetch_hpos)
		update_fetch(hpos);
	last_fetch_hpos = hpos;
}

/* This function is responsible for turning on datafetch if necessary.  */
static inline void decide_line(int hpos) {
	if (hpos <= last_decide_line_hpos)
		return;
	if (fetch_state != fetch_not_started)
		return;

	/* Test if we passed the start of the DDF window.  */
	if (last_decide_line_hpos < plfstrt && hpos >= plfstrt) {
		/* First, take care of the vertical DIW.  Surprisingly enough, this seems to be
		   correct here - putting this into decide_diw() results in garbage.  */
		if (diwstate == DIW_waiting_start && vpos == plffirstline)
			diwstate = DIW_waiting_stop;
		if (diwstate == DIW_waiting_stop && vpos == plflastline)
			diwstate = DIW_waiting_start;

		/* If DMA isn't on by the time we reach plfstrt, then there's no
		   bitplane DMA at all for the whole line.  */
		if (dmaen(DMA_BITPLANE) && diwstate == DIW_waiting_stop) {
			fetch_state = fetch_started;
			fetch_cycle = 0;
			last_fetch_hpos = plfstrt;
			out_nbits = 0;
			out_offs = 0;
			toscr_nbits = 0;
			compute_toscr_delay_0();

			/* If someone already wrote BPL1DAT, clear the area between that point and
			   the real fetch start.  */
			if (amigaDrawEnabled) {
				if (currLineDecision.plfleft != -1) {
					out_nbits = (plfstrt - currLineDecision.plfleft) << (1 + toscr_res);
					out_offs = out_nbits >> 5;
					out_nbits &= 31;
				}
				update_toscr_planes();
			}
			estimate_last_fetch_cycle (plfstrt);
			last_decide_line_hpos = hpos;
			do_sprites(plfstrt);
			return;
		}
	}

	if (last_decide_line_hpos < 0x34)
		do_sprites(hpos);

	last_decide_line_hpos = hpos;
}

/* Called when a color is about to be changed (write to a color register),
 * but the new color has not been entered into the table yet. */
static void record_color_change(int hpos, int regno, uint value) {
	/* Early positions don't appear on-screen. */
	if (!amigaDrawEnabled || vpos < minfirstline || hpos < 0x18)
		return;

	decide_diw(hpos);
	decide_line(hpos);
	remember_ctable();

	curr_color_changes[next_color_change].linepos = hpos;
	curr_color_changes[next_color_change].regno = regno;
	curr_color_changes[next_color_change++].value = value;
}

typedef int sprbuf_res_t, cclockres_t, hwres_t, bplres_t;

static void expand_sprres() {
	/* ECS defaults (LORES,HIRES=140ns,SHRES=70ns) */
	switch ((bplcon3 >> 6) & 3) {
	case 0: sprres = RES_LORES; break;
	case 1: sprres = RES_LORES; break;
	case 2: sprres = RES_HIRES; break;
	case 3: sprres = RES_SUPERHIRES; break;
	}
}

static inline void record_sprite_1(u16 *_GCCRES_ buf, u32 datab, int num) {
	while (datab) {
		u32 tmp = *buf;
		u32 col = (datab & 3) << (2 * num);
		tmp |= col;
		*buf++ = tmp;
		datab >>= 2;
	}
}

/* DATAB contains the sprite data; 16 pixels in two-bit packets.  Bits 0/1
   determine the color of the leftmost pixel, bits 2/3 the color of the next
   etc.
   This function assumes that for all sprites in a given line, SPRXP either
   stays equal or increases between successive calls.

   The data is recorded either in lores pixels (if ECS), or in hires pixels
   (if AJA).  No support for SHRES sprites.  */

static void record_sprite(int num, int sprxp, u16 *_GCCRES_ data, u16 *_GCCRES_ datb, uint ctl) {
	struct sprite_entry *e = curr_sprite_entries + next_sprite_entry;

	/* Try to coalesce entries if they aren't too far apart.  */
	if (! next_sprite_forced && e[-1].max + 16 >= sprxp) {
		e--;
	} else {
		next_sprite_entry++;
		e->pos = sprxp;
		e->has_attached = 0;
	}

	if (sprxp < e->pos)
		return;

	e->max = sprxp + sprite_width;
	e[1].first_pixel = e->first_pixel + ((e->max - e->pos + 3) & ~3);
	next_sprite_forced = 0;

	int word_offs = e->first_pixel + sprxp - e->pos;

	for (int i = 0; i < sprite_width; i += 16) {
		uint da = *data;
		uint db = *datb;
		u32 datab = (sprtaba[da & 0xFF] << 16) | sprtaba[da >> 8];
		datab |= (sprtabb[db & 0xFF] << 16) | sprtabb[db >> 8];

		u16 *buf = spixels + word_offs + (i << 0);
		record_sprite_1(buf, datab, num);

		data++;
		datb++;
	}

	/* We have 8 bits per pixel in spixstate, two for every sprite pair.  The
	   low order bit records whether the attach bit was set for this pair.  */

	if (ctl & (num << 7) & 0x80) {
		u32 state = 0x01010101 << (num - 1);
		u8 *stb1 = spixstate.bytes + word_offs;
		for (int i = 0; i < sprite_width; i += 8) {
			stb1[0] |= state;
			stb1[1] |= state;
			stb1[2] |= state;
			stb1[3] |= state;
			stb1[4] |= state;
			stb1[5] |= state;
			stb1[6] |= state;
			stb1[7] |= state;
			stb1 += 8;
		}
		e->has_attached = 1;
	}
}

static void decide_sprites(int hpos) {
	int nrs[MaxSprites], posns[MaxSprites];
	int point = hpos << 1;
	int window_width = sprite_width >> sprres;

	if (!amigaDrawEnabled || hpos < 0x14 || point == last_sprite_point)
		return;

	decide_diw (hpos);
	decide_line (hpos);

	int count = 0;
	for (int i = 0; i < MaxSprites; i++) {
		int sprxp = spr[i].xpos;
		int hw_xp = (sprxp >> sprite_buffer_res);
		int window_xp = coord_hw_to_window_x(hw_xp) + (DIW_DDF_OFFSET);

		if (! spr[i].armed || sprxp < 0 || hw_xp <= last_sprite_point || hw_xp > point)
			continue;
		if ((currLineDecision.diwfirstword >= 0 && window_xp + window_width < currLineDecision.diwfirstword)
			|| (currLineDecision.diwlastword >= 0 && window_xp > currLineDecision.diwlastword))
			continue;

		/* Sort the sprites in order of ascending X position before recording them.  */
		int bestp = 0;
		for (; bestp < count; bestp++) {
			if (posns[bestp] > sprxp)
				break;
			if (posns[bestp] == sprxp && nrs[bestp] < i)
				break;
		}
		int j = count;
		for (; j > bestp; j--) {
			posns[j] = posns[j-1];
			nrs[j] = nrs[j-1];
		}
		posns[j] = sprxp;
		nrs[j] = i;
		count++;
	}
	for (int i = 0; i < count; i++) {
		int nr = nrs[i];
		record_sprite(nr, spr[nr].xpos, sprdata[nr], sprdatb[nr], sprctl[nr]);
	}
	last_sprite_point = point;
}

/* End of a horizontal scan line. Finish off all decisions that were not
 * made yet. */
static inline void finish_decisions() {
	struct draw_info *dip;
	struct decision *dp;
	int hpos = currentHPos();

	if (!amigaDrawEnabled)
		return;

	decide_diw(hpos);
	decide_line(hpos);
	decide_fetch(hpos);

	if (currLineDecision.plfleft != -1 && currLineDecision.plflinelen == -1) {
		if (fetch_state != fetch_not_started)
			return;
		currLineDecision.plfright = currLineDecision.plfleft;
		currLineDecision.plflinelen = 0;
		currLineDecision.bplres = RES_LORES;
	}

	/* Large DIWSTOP values can cause the stop position never to be
	 * reached, so the state machine always stays in the same state and
	 * there's a more-or-less full-screen DIW. */
	if (hdiwstate == DIW_waiting_stop || currLineDecision.diwlastword > max_diwlastword)
		currLineDecision.diwlastword = max_diwlastword;

	dip = curr_drawinfo + next_lineno;
	dp = line_decisions + next_lineno;

	if (currLineDecision.plfleft != -1)
		decide_sprites(hpos);

	dip->last_sprite_entry = next_sprite_entry;
	dip->last_color_change = next_color_change;

	remember_ctable();

	dip->nr_color_changes = next_color_change - dip->first_color_change;
	dip->nr_sprites = next_sprite_entry - dip->first_sprite_entry;
	*dp = currLineDecision;
}

/* Set the state of all decisions to "undecided" for a new scanline. */
static inline void reset_decisions() {
	currLineDecision.nr_planes = 0;
	currLineDecision.plfleft = -1;
	currLineDecision.plflinelen = -1;

	/* decided_res shouldn't be touched before it's initialized by decide_line(). */
	currLineDecision.diwfirstword = -1;
	currLineDecision.diwlastword = -2;
	if (hdiwstate == DIW_waiting_stop)
		currLineDecision.diwfirstword = 0;
	currLineDecision.ctable = -1;

	curr_drawinfo[next_lineno].first_color_change = next_color_change;
	curr_drawinfo[next_lineno].first_sprite_entry = next_sprite_entry;
	next_sprite_forced = 1;

	last_sprite_point = 0;
	fetch_state = fetch_not_started;
	passed_plfstop = 0;

	u32 *ptr0 = todisplay;
	u32 *ptr1 = fetched;
	u32 *ptr2 = outword;
	for (int i=0;i<MAX_PLANES;i++,ptr0++,ptr1++,ptr2++) {
		*ptr0=0;
		*ptr1=0;
		*ptr2=0;
	}
	last_decide_line_hpos = -1;
	last_diw_pix_hpos = -1;
	last_ddf_pix_hpos = -1;
	last_sprite_hpos = -1;
	last_fetch_hpos = -1;
}


/* set PAL or NTSC timing variables */

static void init_hz(bool pal) {
	if (pal) {
		maxvpos = MAXVPOS_PAL;
		maxhpos = MAXHPOS_PAL;
		minfirstline = MINFIRSTLINE_PAL;
		vblank_hz = VBLANK_HZ_PAL;
	} else {
		maxvpos = MAXVPOS_NTSC;
		maxhpos = MAXHPOS_NTSC;
		minfirstline = MINFIRSTLINE_NTSC;
		vblank_hz = VBLANK_HZ_NTSC;
	}
	qDebug("Using %s timing\n", pal ? "PAL" : "NTSC");
}

static void calcdiw() {
	int hstrt = diwstrt & 0xFF;
	int hstop = diwstop & 0xFF;
	int vstrt = diwstrt >> 8;
	int vstop = diwstop >> 8;

	hstop += 0x100;
	if ((vstop & 0x80) == 0)
		vstop |= 0x100;

	diwfirstword = coord_diw_to_window_x (hstrt);
	diwlastword = coord_diw_to_window_x (hstop);
	if (diwfirstword < 0)
		diwfirstword = 0;

	plffirstline = vstrt;
	plflastline = vstop;

	plfstrt = ddfstrt;
	plfstop = ddfstop;
	if (plfstrt < 0x18)
		plfstrt = 0x18;
}

 /*
  * register functions
  */
static __inline__ u16 DENISEID() {
	return 0xFFFF;
}

static inline u16 DMACONR() {
	return (dmacon | (bltstate==BLT_done ? 0 : 0x4000)
		| (blt_info.blitzero ? 0x2000 : 0));
}

static inline u16 INTENAR() {
	return intena;
}

u16 INTREQR() {
	return intreq;
}

static inline u16 ADKCONR() {
	return adkcon;
}

static inline u16 VPOSR() {
	return (vpos >> 8) | lof;
}

static inline void VPOSW(u16 v) {
	lof = v & 0x8000;
	/*
	 * This register is much more fun on a real Amiga. You can program
	 * refresh rates with it ;) But I won't emulate this...
	 */
}

#define VHPOSR() ((vpos << 8) | currentHPos ())
#define COP1LCH(V) cop1lc = (cop1lc & 0xFFFF) | ((u32)V << 16)
#define COP1LCL(V) cop1lc = (cop1lc & ~0xFFFF) | (V & 0xFFFE)
#define COP2LCH(V) cop2lc = (cop2lc & 0xFFFF) | ((u32)V << 16)
#define COP2LCL(V) cop2lc = (cop2lc & ~0xFFFF) | (V & 0xFFFE)

static void copperStart() {
	if (amigaEvents[AmigaEventCopper].active) {
		amigaEvents[AmigaEventCopper].active = 0;
		amigaEventsSchedule();
	}
	cop_state.ignore_next = 0;
	cop_state.state = COP_read1;
	cop_state.vpos = vpos;
	cop_state.hpos = currentHPos() & ~1;

	if (dmaen(DMA_COPPER)) {
		coppperEnabledCurrLine = 1;
		amigaCpuSetSpcFlag(SpcFlagCopper);
	}
}

static inline void COPJMP1(u16 a) {
	Q_UNUSED(a)
	cop_state.ip = cop1lc;
	copperStart();
}

static inline void COPJMP2(u16 a) {
	Q_UNUSED(a)
	cop_state.ip = cop2lc;
	copperStart();
}

static inline void COPCON(u16 a) {
	copcon = a;
}

static inline void DMACON(int hpos, u16 v) {
	u16 oldcon = dmacon;

	decide_line(hpos);
	decide_fetch(hpos);

	setclr(&dmacon, v);
	dmacon &= 0x1FFF;

	/* FIXME? Maybe we need to think a bit more about the master DMA enable
	 * bit in these cases. */
	if ((dmacon & DMA_COPPER) != (oldcon & DMA_COPPER)) {
		amigaEvents[AmigaEventCopper].active = 0;
	}
	if ((dmacon & DMA_COPPER) > (oldcon & DMA_COPPER)) {
		cop_state.ip = cop1lc;
		cop_state.ignore_next = 0;
		cop_state.state = COP_read1;
		cop_state.vpos = vpos;
		cop_state.hpos = hpos & ~1;
		coppperEnabledCurrLine = 1;
		amigaCpuSetSpcFlag(SpcFlagCopper);
	}
	if (!(dmacon & DMA_COPPER)) {
		coppperEnabledCurrLine = 0;
		amigaCpuClearSpcFlag (SpcFlagCopper);
		cop_state.state = COP_stop;
	}

	if ((dmacon & DMA_BLITPRI) > (oldcon & DMA_BLITPRI) && bltstate != BLT_done) {
		static int count = 0;
		if (!count) {
			count = 1;
			qDebug("warning: program is doing blitpri hacks.\n");
		}
		setNasty();
	}
	if ((dmacon & (DMA_BLITPRI | DMA_BLITTER | DMA_MASTER)) != (DMA_BLITPRI | DMA_BLITTER | DMA_MASTER))
		amigaCpuClearSpcFlag(SpcFlagBltNasty);

	amigaSpuUpdate();
	amigaSpuCheckDma();
	amigaSpuSchedule();
	amigaEventsSchedule();
}


static inline void SET_INTERRUPT() {
	int new_irqs = 0, new_level = 0;

	if (intena & 0x4000) {
		int imask = intreq & intena;
		if (imask & 0x0007) { new_irqs |= 1 << 1; new_level = 1; }
		if (imask & 0x0008) { new_irqs |= 1 << 2; new_level = 2; }
		if (imask & 0x0070) { new_irqs |= 1 << 3; new_level = 3; }
		if (imask & 0x0780) { new_irqs |= 1 << 4; new_level = 4; }
		if (imask & 0x1800) { new_irqs |= 1 << 5; new_level = 5; }
		if (imask & 0x2000) { new_irqs |= 1 << 6; new_level = 6; }
	}

	if (new_irqs == amigaCpuInterrupts) {
		// nothing changed
	} else if (new_irqs == 0) {
		amigaCpuInterrupts = 0;
		amigaCpuIrqUpdate(0);
	} else {
		int old_irqs = amigaCpuInterrupts;
		int old_level = 0;
		int end_timeslice = 0;

		for (old_irqs>>=1; old_irqs; old_irqs>>=1, old_level++);
			end_timeslice = new_level > old_level && new_level > amigaCpuIntMask();

		amigaCpuInterrupts = new_irqs;
		amigaCpuIrqUpdate(end_timeslice);
	}
}

static inline void INTENA (u16 v) {
	setclr(&intena,v);
	/* There's stupid code out there that does
	[some INTREQ bits at level 3 are set]
	clear all INTREQ bits
	Enable one INTREQ level 3 bit
	Set level 3 handler */
	SET_INTERRUPT();
}

static inline void INTREQ_COMMON(u16 VCLR) {
	setclr(&intreq,VCLR);
	SET_INTERRUPT();
}

void INTREQ_0(u16 v) {
	INTREQ_COMMON(v);
}

void INTREQ(u16 v) {
	INTREQ_COMMON(v);
	amigaCiaRethink();
}

static inline void ADKCON(u16 v) {
	amigaSpuUpdate();
	setclr(&adkcon,v);
	amigaSpuUpdateAdkMasks();
}

static inline void BPLPTH(int hpos, u16 v, int num) {
	decide_line(hpos);
	decide_fetch(hpos);
	bplpt[num] = (bplpt[num] & 0xFFFF) | ((u32)v << 16);
}

static inline void BPLPTL(int hpos, u16 v, int num) {
	decide_line(hpos);
	decide_fetch(hpos);
	bplpt[num] = (bplpt[num] & ~0xFFFF) | (v & 0xFFFE);
}

static inline void BPLCON0(int hpos, u16 v) {
	v &= ~0x00F1;

	if (bplcon0 == v)
		return;
	decide_line (hpos);
	decide_fetch (hpos);

	/* HAM change?  */
	if ((bplcon0 ^ v) & 0x800)
		record_color_change (hpos, -1, !! (v & 0x800));
	bplcon0 = v;
	bplcon0Planes = GET_PLANES(v);
	bplcon0Resolution = GET_RES(v);
	curr_diagram = cycle_diagram_table[bplcon0Resolution][bplcon0Planes];
	expand_fmodes();
}

static inline void BPLCON1(int hpos, u16 v) {
	if (bplcon1 == v)
		return;
	decide_line(hpos);
	decide_fetch(hpos);
	bplcon1 = v;
}

static inline void BPLCON2(int hpos, u16 v) {
	if (bplcon2 == v)
		return;
	decide_line(hpos);
	bplcon2 = v;
}

#define BPLCON3(HPOS,V)
#define BPLCON4(HPOS,V)

static inline void BPL1MOD(int hpos, u16 v) {
	v &= ~1;
	if ((s16)bpl1mod == (s16)v)
		return;
	decide_line (hpos);
	decide_fetch (hpos);
	bpl1mod = v;
}

static inline void BPL2MOD(int hpos, u16 v) {
	v &= ~1;
	if ((s16)bpl2mod == (s16)v)
		return;
	decide_line(hpos);
	decide_fetch(hpos);
	bpl2mod = v;
}

static inline void BPL1DAT(int hpos, u16 v) {
	decide_line(hpos);
	bpl1dat = v;
	maybe_first_bpl1dat(hpos);
}

static inline void BPL2DAT(u32 V) { Q_UNUSED(V) }
static inline void BPL3DAT(u32 V) { Q_UNUSED(V) }
static inline void BPL4DAT(u32 V) { Q_UNUSED(V) }
static inline void BPL5DAT(u32 V) { Q_UNUSED(V) }
static inline void BPL6DAT(u32 V) { Q_UNUSED(V) }
static inline void BPL7DAT(u32 V) { Q_UNUSED(V) }
static inline void BPL8DAT(u32 V) { Q_UNUSED(V) }

static inline void DIWSTRT(int hpos, u16 v) {
	if (diwstrt == v)
		return;
	decide_line(hpos);
	diwstrt = v;
	calcdiw();
}

static inline void DIWSTOP(int hpos, u16 v) {
	if (diwstop == v)
		return;
	decide_line(hpos);
	diwstop = v;
	calcdiw();
}

#define DIWHIGH(HPOS,V)

static inline void DDFSTRT(int hpos, u16 v) {
	v &= 0xFC;
	if (ddfstrt == v)
		return;
	decide_line(hpos);
	ddfstrt = v;
	calcdiw();
	if (ddfstop > 0xD4 && (ddfstrt & 4) == 4) {
		static int last_warned;
		last_warned = (last_warned + 1) & 4095;
		if (last_warned == 0)
			qDebug("WARNING! Very strange DDF values.\n");
	}
}

static inline void DDFSTOP(int hpos, u16 v) {
	/* ??? "Virtual Meltdown" sets this to 0xD2 and expects it to behave
	   differently from 0xD0.  RSI Megademo sets it to 0xd1 and expects it
	   to behave like 0xd0.  Some people also write the high 8 bits and
	   expect them to be ignored.  So mask it with 0xFE.  */
	v &= 0xFE;
	if (ddfstop == v)
		return;
	decide_line(hpos);
	decide_fetch(hpos);
	ddfstop = v;
	calcdiw();
	if (fetch_state != fetch_not_started)
		estimate_last_fetch_cycle(hpos);
	if (ddfstop > 0xD4 && (ddfstrt & 4) == 4)
		qDebug("WARNING! Very strange DDF values.\n");
}

static inline void FMODE(u16 v) {
	Q_UNUSED(v)
	sprite_width = GET_SPRITEWIDTH(0);
	curr_diagram = cycle_diagram_table[0][bplcon0Planes];
	expand_fmodes();
}

static inline void BLTADAT(u16 v) {
	maybe_blit(0);
	blt_info.bltadat = v;
}
/*
 * "Loading data shifts it immediately" says the HRM. Well, that may
 * be true for BLTBDAT, but not for BLTADAT - it appears the A data must be
 * loaded for every word so that AFWM and ALWM can be applied.
 */
static inline void BLTBDAT(u16 v) {
	maybe_blit(0);
	if (bltcon1 & 2)
		blt_info.bltbhold = v << (bltcon1 >> 12);
	else
		blt_info.bltbhold = v >> (bltcon1 >> 12);
	blt_info.bltbdat = v;
}


static inline void BLTCDAT(u16 V) { maybe_blit(0); blt_info.bltcdat = V; }
static inline void BLTAMOD(u16 V) { maybe_blit(1); blt_info.bltamod = (s16)(V & 0xFFFE); }
static inline void BLTBMOD(u16 V) { maybe_blit(1); blt_info.bltbmod = (s16)(V & 0xFFFE); }
static inline void BLTCMOD(u16 V) { maybe_blit(1); blt_info.bltcmod = (s16)(V & 0xFFFE); }
static inline void BLTDMOD(u16 V) { maybe_blit(1); blt_info.bltdmod = (s16)(V & 0xFFFE); }
static inline void BLTCON0(u16 V) { maybe_blit(0); bltcon0 = V; blinea_shift = V >> 12; }
static inline void BLTCON0L(u16 V) { Q_UNUSED(V) }
static inline void BLTCON1(u16 V) { maybe_blit(0); bltcon1 = V; }
static inline void BLTAFWM(u16 V) { maybe_blit(0); blt_info.bltafwm = V; }
static inline void BLTALWM(u16 V) { maybe_blit(0); blt_info.bltalwm = V; }
static inline void BLTAPTH(u16 V) { maybe_blit(0); bltapt = (bltapt & 0xFFFF) | ((u32)V << 16); }
static inline void BLTAPTL(u16 V) { maybe_blit(0); bltapt = (bltapt & ~0xFFFF) | (V & 0xFFFE); }
static inline void BLTBPTH(u16 V) { maybe_blit(0); bltbpt = (bltbpt & 0xFFFF) | ((u32)V << 16); }
static inline void BLTBPTL(u16 V) { maybe_blit(0); bltbpt = (bltbpt & ~0xFFFF) | (V & 0xFFFE); }
static inline void BLTCPTH(u16 V) { maybe_blit(0); bltcpt = (bltcpt & 0xFFFF) | ((u32)V << 16); }
static inline void BLTCPTL(u16 V) { maybe_blit(0); bltcpt = (bltcpt & ~0xFFFF) | (V & 0xFFFE); }
static inline void BLTDPTH(u16 V) { maybe_blit(0); bltdpt = (bltdpt & 0xFFFF) | ((u32)V << 16); }
static inline void BLTDPTL(u16 V) { maybe_blit(0); bltdpt = (bltdpt & ~0xFFFF) | (V & 0xFFFE); }

static void BLTSIZE(u16 v) {
	maybe_blit(0);
	blt_info.vblitsize = v >> 6;
	blt_info.hblitsize = v & 0x3F;
	if (!blt_info.vblitsize)
		blt_info.vblitsize = 1024;
	if (!blt_info.hblitsize)
		blt_info.hblitsize = 64;
	bltstate = BLT_init;
	do_blitter();
}

#define BLTSIZV(V)
#define BLTSIZH(V)

static inline void SPRxCTL_1(u16 v, int num) {
	AmigaSprite *s = &spr[num];

	sprctl[num] = v;
	s->armed = 0;

	int sprxp = ((sprpos[num] & 0xFF) << 1) + (v & 1);
	/* Quite a bit salad in this register... */
	s->xpos = sprxp;
	s->vstart = (sprpos[num] >> 8) | ((sprctl[num] << 6) & 0x100);
	s->vstop = (sprctl[num] >> 8) | ((sprctl[num] << 7) & 0x100);
	if (vpos == s->vstart)
		s->state = AmigaSprite::WaitingStop;
}

static inline void SPRxPOS_1(u16 v, int num) {
	AmigaSprite *s = &spr[num];

	sprpos[num] = v;

	int sprxp = ((v & 0xFF) << 1) + (sprctl[num] & 1);

	s->xpos = sprxp;
	s->vstart = (sprpos[num] >> 8) | ((sprctl[num] << 6) & 0x100);
}

static inline void SPRxDATA_1(u16 v, int num) {
	sprdata[num][0] = v;
	spr[num].armed = 1;
}

#define SPRxDATB_1(V,NUM) sprdatb[NUM][0] = V

#define SPRxDATA(HPOS,V,NUM) decide_sprites(HPOS); SPRxDATA_1(V, NUM)
#define SPRxDATB(HPOS,V,NUM) decide_sprites(HPOS); SPRxDATB_1(V, NUM)
#define SPRxCTL(HPOS,V,NUM) decide_sprites(HPOS); SPRxCTL_1(V, NUM)
#define SPRxPOS(HPOS,V,NUM) decide_sprites(HPOS); SPRxPOS_1(V, NUM)

static inline void SPRxPTH(int hpos, u16 v, int num) {
	decide_sprites(hpos);
	spr[num].pt &= 0xFFFF;
	spr[num].pt |=(u32)v << 16;
}
static inline void SPRxPTL(int hpos, u16 v, int num) {
	decide_sprites(hpos);
	spr[num].pt &= ~0xFFFF;
	spr[num].pt |= v;
}

static inline void CLXCON(u16 v) { Q_UNUSED(v) }
static inline void CLXCON2(u16 v) { Q_UNUSED(v) }

static inline u16 CLXDAT() {
	static int count = 0;
	if (!count) {
		count++;
		qDebug("Warning: CLXDAT not supported!");
	}
	return 0;
}

#define COLOR_READ(NUM) 0xFFFF

static inline void COLOR_WRITE(int hpos, u16 v, int num) {
	v &= 0xFFF;
	if (currentColors.color_uae_regs_ecs[num] == v)
		return;
	/* Call this with the old table still intact. */
	record_color_change(hpos, num, v);
	remembered_color_entry = -1;
	currentColors.color_uae_regs_ecs[num] = v;
	currentColors.acolors[num] = xcolors[v];
}

static inline void POTGO(u16 v) {
	potgo = v;
}

static inline u16 POTGOR() {
	u16 v = (potgo | (potgo >> 1)) & 0x5500;

	v |= (~potgo & 0xAA00) >> 1;

	if (amigaInputPortButtons[0] & 2)
		v &= ~0x0400;
	if (amigaInputPortButtons[0] & 4)
		v &= ~0x0100;
	if (amigaInputPortButtons[1] & 2)
		v &= ~0x4000;
	if (amigaInputPortButtons[1] & 4)
		v &= ~0x1000;
	return v;
}

static inline u16 POT0DAT() {
	// TODO static var
	static u16 cnt = 0;

	if (amigaInputPortButtons[0] & 2)
		cnt = ((cnt + 1) & 0xFF) | (cnt & 0xFF00);

	if (amigaInputPortButtons[0] & 4)
		cnt += 0x100;

	return cnt;
}

static inline u16 JOY0DAT() {
	return amigaInputPortDir[0];
}

static inline u16 JOY1DAT() {
	return amigaInputPortDir[1];
}

static inline void JOYTEST(u16 v) {
	amigaInputPortDir[0] = v & 0xFC;
	amigaInputPortDir[0] |= ((v >> 8) & 0xFC) << 8;
	amigaInputPortDir[1] = amigaInputPortDir[0];
}

/* The copper code.  The biggest nightmare in the whole emulator.

   Alright.  The current theory:
   1. Copper moves happen 2 cycles after state READ2 is reached.
	  It can't happen immediately when we reach READ2, because the
	  data needs time to get back from the bus.  An additional 2
	  cycles are needed for non-Agnus registers, to take into account
	  the delay for moving data from chip to chip.
   2. As stated in the HRM, a WAIT really does need an extra cycle
	  to wake up.  This is implemented by _not_ falling through from
	  a successful wait to READ1, but by starting the next cycle.
	  (Note: the extra cycle for the WAIT apparently really needs a
	  free cycle; i.e. contention with the bitplane fetch can slow
	  it down).
   3. Apparently, to compensate for the extra wake up cycle, a WAIT
	  will use the _incremented_ horizontal position, so the WAIT
	  cycle normally finishes two clocks earlier than the position
	  it was waiting for.  The extra cycle then takes us to the
	  position that was waited for.
	  If the earlier cycle is busy with a bitplane, things change a bit.
	  E.g., waiting for position 0x50 in a 6 plane display: In cycle
	  0x4e, we fetch BPL5, so the wait wakes up in 0x50, the extra cycle
	  takes us to 0x54 (since 0x52 is busy), then we have READ1/READ2,
	  and the next register write is at 0x5c.
   4. The last cycle in a line is not usable for the copper.
   5. A 4 cycle delay also applies to the WAIT instruction.  This means
	  that the second of two back-to-back WAITs (or a WAIT whose
	  condition is immediately true) takes 8 cycles.
   6. This also applies to a SKIP instruction.  The copper does not
	  fetch the next instruction while waiting for the second word of
	  a WAIT or a SKIP to arrive.
   7. A SKIP also seems to need an unexplained additional two cycles
	  after its second word arrives; this is _not_ a memory cycle (I
	  think, the documentation is pretty clear on this).
   8. Two additional cycles are inserted when writing to COPJMP1/2.  */

/* Determine which cycles are available for the copper in a display
 * with a agiven number of planes.  */

static inline int copperCantRead(int hpos) {
	if (hpos + 1 >= maxhpos)
		return 1;
	if (fetch_state == fetch_not_started || hpos < currLineDecision.plfleft)
		return 0;
	if ((passed_plfstop == 3 && hpos >= currLineDecision.plfright)
			|| hpos >= estimated_last_fetch_cycle)
		return 0;
	return curr_diagram[(hpos + cycle_diagram_shift) & fetchstart_mask];
}

static inline int dangerousReg(int reg) {
	/* Safe:
	 * Bitplane pointers, control registers, modulos and data.
	 * Sprite pointers, control registers, and data.
	 * Color registers.  */
	if (reg >= 0xE0 && reg < 0x1C0)
		return 0;
	return 1;
}

/* The future, Conan?
   We try to look ahead in the copper list to avoid doing continuous calls
   to updat_copper (which is what happens when SpcFlagCopper is set).  If
   we find that the same effect can be achieved by setting a delayed event
   and then doing multiple copper insns in one batch, we can get a massive
   speedup.

   We don't try to be precise here.  All copper reads take exactly 2 cycles,
   the effect of bitplane contention is ignored.  Trying to get it exactly
   right would be much more complex and as such carry a huge risk of getting
   it subtly wrong; and it would also be more expensive - we want this code
   to be fast.  */

static inline void copperPredict() {
	u32 ip = cop_state.ip;
	uint c_hpos = cop_state.hpos;
	enum copper_states state = cop_state.state;
	uint w1, w2, cycle_count;

	switch (state) {
	case COP_read1_wr_in2:
	case COP_read2_wr_in2:
	case COP_read1_wr_in4:
		if (dangerousReg(cop_state.saved_i1))
			return;
		state = state == COP_read2_wr_in2 ? COP_read2 : COP_read1;
		break;

	case COP_read1_in2:
		c_hpos += 2;
		state = COP_read1;
		break;

	case COP_stop:
	case COP_bltwait:
	case COP_wait1:
	case COP_skip_in4:
	case COP_skip_in2:
		return;

	case COP_wait_in4:
		c_hpos += 2;
		/* fallthrough */
	case COP_wait_in2:
		c_hpos += 2;
		/* fallthrough */
	case COP_wait:
		state = COP_wait;
		break;

	default:
		break;
	}
	/* Only needed for COP_wait, but let's shut up the compiler.  */
	w1 = cop_state.saved_i1;
	w2 = cop_state.saved_i2;
	cop_state.first_sync = c_hpos;
	cop_state.regtypes_modified = REGTYPE_FORCE;

	/* Get this case out of the way, so that the loop below only has to deal
	   with read1 and wait.  */
	if (state == COP_read2) {
		w1 = cop_state.i1;
		if (w1 & 1) {
			w2 = amigaMemChipGetWord(ip);
			if (w2 & 1)
				goto done;
			state = COP_wait;
			c_hpos += 4;
		} else if (dangerousReg(w1)) {
			c_hpos += 4;
			goto done;
		} else {
			cop_state.regtypes_modified |= regtypes[w1 & 0x1FE];
			state = COP_read1;
			c_hpos += 2;
		}
		ip += 2;
	}

	while (c_hpos + 1 < maxhpos) {
		if (state == COP_read1) {
			w1 = amigaMemChipGetWord(ip);
			if (w1 & 1) {
				w2 = amigaMemChipGetWord(ip+2);
				if (w2 & 1)
					break;
				state = COP_wait;
				c_hpos += 6;
			} else if (dangerousReg(w1)) {
				c_hpos += 6;
				goto done;
			} else {
				cop_state.regtypes_modified |= regtypes[w1 & 0x1FE];
				c_hpos += 4;
			}
			ip += 4;
		} else if (state == COP_wait) {
			if ((w2 & 0xFE) != 0xFE) {
				break;
			} else {
				uint vcmp = (w1 & (w2 | 0x8000)) >> 8;
				uint hcmp = (w1 & 0xFE);
				uint vp = vpos & (((w2 >> 8) & 0x7F) | 0x80);

				if (vp < vcmp) {
					/* Whee.  We can wait until the end of the line!  */
					c_hpos = maxhpos;
				} else if (vp > vcmp || hcmp <= c_hpos) {
					state = COP_read1;
					/* minimum wakeup time */
					c_hpos += 2;
				} else {
					state = COP_read1;
					c_hpos = hcmp;
				}
				/* If this is the current instruction, remember that we don't
				   need to sync CPU and copper anytime soon.  */
				if (cop_state.ip == ip)
					cop_state.first_sync = c_hpos;
			}
		} else {
			return;
		}
	}

  done:
	cycle_count = c_hpos - cop_state.hpos;
	if (cycle_count >= 8) {
		amigaCpuClearSpcFlag(SpcFlagCopper);
		amigaEvents[AmigaEventCopper].active = 1;
		amigaEvents[AmigaEventCopper].time = amigaCycles + cycle_count * CYCLE_UNIT;
		amigaEventsSchedule();
	}
}

static inline void copperPerformWrite(int oldHpos) {
	u32 addr = cop_state.saved_i1 & 0x1FE;

	if (addr < (copcon & 2 ? (0x40u) : 0x80u)) {
		cop_state.state = COP_stop;
		coppperEnabledCurrLine = 0;
		amigaCpuClearSpcFlag(SpcFlagCopper);
		return;
	}

	if (addr == 0x88) {
		cop_state.ip = cop1lc;
		cop_state.state = COP_read1_in2;
	} else if (addr == 0x8A) {
		cop_state.ip = cop2lc;
		cop_state.state = COP_read1_in2;
	} else {
		custom_wput_1(oldHpos, addr, cop_state.saved_i2);
	}
}

static const int isagnus[]= {
	1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
	1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* BPLxPT */
	0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* SPRxPT */
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* colors */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static inline void copperUpdate(int until_hpos) {
	int vp = vpos & (((cop_state.saved_i2 >> 8) & 0x7F) | 0x80);
	int c_hpos = cop_state.hpos;

	if (amigaEvents[AmigaEventCopper].active)
		return;

	if (cop_state.state == COP_wait && vp < cop_state.vcmp)
		return;

	until_hpos &= ~1;
	if (until_hpos > (maxhpos & ~1))
		until_hpos = maxhpos & ~1;
	until_hpos += 2;

	for (;;) {
	int old_hpos = c_hpos;
	int hp;

	if (c_hpos >= until_hpos)
		break;

	/* So we know about the fetch state.  */
	decide_line(c_hpos);

	switch (cop_state.state) {
	case COP_read1_in2:
		cop_state.state = COP_read1;
		break;
	case COP_read1_wr_in2:
		cop_state.state = COP_read1;
		copperPerformWrite (old_hpos);
		/* That could have turned off the copper.  */
		if (! coppperEnabledCurrLine)
			goto out;
		break;
	case COP_read1_wr_in4:
		cop_state.state = COP_read1_wr_in2;
		break;
	case COP_read2_wr_in2:
		cop_state.state = COP_read2;
		copperPerformWrite(old_hpos);
		/* That could have turned off the copper.  */
		if (! coppperEnabledCurrLine)
			goto out;
		break;
	case COP_wait_in2:
		cop_state.state = COP_wait1;
		break;
	case COP_wait_in4:
		cop_state.state = COP_wait_in2;
		break;
	case COP_skip_in2: {
		static int skipped_before;
		unsigned int vcmp, hcmp, vp1, hp1;
		cop_state.state = COP_read1_in2;

		vcmp = (cop_state.saved_i1 & (cop_state.saved_i2 | 0x8000)) >> 8;
		hcmp = (cop_state.saved_i1 & cop_state.saved_i2 & 0xFE);

		if (!skipped_before) {
			skipped_before = 1;
			qDebug("Program uses Copper SKIP instruction.\n");
		}
		vp1 = vpos & (((cop_state.saved_i2 >> 8) & 0x7F) | 0x80);
		hp1 = old_hpos & (cop_state.saved_i2 & 0xFE);

		bool ab = (vp1 > vcmp || (vp1 == vcmp && hp1 >= hcmp));
		bool bb = ((cop_state.saved_i2 & 0x8000) != 0 || !(DMACONR() & 0x4000));
		if (ab & bb)
			cop_state.ignore_next = 1;
		break;
	}
	case COP_skip_in4:
		cop_state.state = COP_skip_in2;
		break;
	default:
		break;
	}

	c_hpos += 2;
	if (copperCantRead (old_hpos))
		continue;

	switch (cop_state.state) {
	case COP_read1_wr_in4:
		return;

	case COP_read1_wr_in2:
	case COP_read1:
		cop_state.i1 = amigaMemChipGetWord(cop_state.ip);
		cop_state.ip += 2;
		cop_state.state = (cop_state.state == COP_read1 ? COP_read2 : COP_read2_wr_in2);
		break;

	case COP_read2_wr_in2:
		return;

	case COP_read2:
		cop_state.i2 = amigaMemChipGetWord(cop_state.ip);
		cop_state.ip += 2;
		if (cop_state.ignore_next) {
			cop_state.ignore_next = 0;
			cop_state.state = COP_read1;
			break;
		}

		cop_state.saved_i1 = cop_state.i1;
		cop_state.saved_i2 = cop_state.i2;

		if (cop_state.i1 & 1) {
			if (cop_state.i2 & 1)
				cop_state.state = COP_skip_in4;
			else
				cop_state.state = COP_wait_in4;
		} else {
			uint reg = cop_state.i1 & 0x1FE;
			cop_state.state = isagnus[reg >> 1] ? COP_read1_wr_in2 : COP_read1_wr_in4;
		}
		break;

	case COP_wait1:
		/* There's a nasty case here.  As stated in the "Theory" comment above, we
		   test ajainst the incremented copper position.  I believe this means that
		   we have to increment the _vertical_ position at the last cycle in the line,
		   and set the horizontal position to 0.
		   Normally, this isn't going to make a difference, since we consider these
		   last cycles unavailable for the copper, so waking up in the last cycle has
		   the same effect as waking up at the start of the line.  However, there is
		   one possible problem:  If we're at 0xFFE0, any wait for an earlier position
		   must _not_ complete (since, in effect, the current position will be back
		   at 0/0).  This can be seen in the Superfrog copper list.
		   Things get monstrously complicated if we try to handle this "properly" by
		   incrementing vpos and setting c_hpos to 0.  Especially the various speedup
		   hacks really assume that vpos remains constant during one line.  Hence,
		   this hack: defer the entire decision until the next line if necessary.  */
		if (c_hpos >= (maxhpos & ~1))
			break;
		cop_state.state = COP_wait;

		cop_state.vcmp = (cop_state.saved_i1 & (cop_state.saved_i2 | 0x8000)) >> 8;
		cop_state.hcmp = (cop_state.saved_i1 & cop_state.saved_i2 & 0xFE);

		vp = vpos & (((cop_state.saved_i2 >> 8) & 0x7F) | 0x80);

		if (cop_state.saved_i1 == 0xFFFF && cop_state.saved_i2 == 0xFFFE) {
			cop_state.state = COP_stop;
			coppperEnabledCurrLine = 0;
			amigaCpuClearSpcFlag(SpcFlagCopper);
			goto out;
		}
		if (vp < cop_state.vcmp) {
			coppperEnabledCurrLine = 0;
			amigaCpuClearSpcFlag(SpcFlagCopper);
			goto out;
		}

		/* fall through */
	case COP_wait:
		if (vp < cop_state.vcmp)
			return;
		hp = c_hpos & (cop_state.saved_i2 & 0xFE);
		if (vp == cop_state.vcmp && hp < cop_state.hcmp) {
			/* Position not reached yet.  */
			if ((cop_state.saved_i2 & 0xFE) == 0xFE) {
				int wait_finish = cop_state.hcmp - 2;
				/* This will leave c_hpos untouched if it's equal to wait_finish.  */
				if (wait_finish < c_hpos)
					return;
				else if (wait_finish <= until_hpos)
					c_hpos = wait_finish;
				else
					c_hpos = until_hpos;
			}
			break;
		}
		/* Now we know that the comparisons were successful.  We might still
		   have to wait for the blitter though.  */
		if ((cop_state.saved_i2 & 0x8000) == 0 && (DMACONR() & 0x4000)) {
			/* We need to wait for the blitter.  */
			cop_state.state = COP_bltwait;
			coppperEnabledCurrLine = 0;
			amigaCpuClearSpcFlag(SpcFlagCopper);
			goto out;
		}
		cop_state.state = COP_read1;
		break;

	default:
		break;
	}
	}

  out:
	cop_state.hpos = c_hpos;

	if ((amigaCpuSpcFlags & SpcFlagCopper) && c_hpos + 8 < maxhpos)
		copperPredict();
}

static inline void computeSpcFlagCopper() {
	coppperEnabledCurrLine = 0;
	amigaCpuClearSpcFlag (SpcFlagCopper);
	if (!dmaen(DMA_COPPER) || cop_state.state == COP_stop || cop_state.state == COP_bltwait)
		return;

	if (cop_state.state == COP_wait) {
		int vp = vpos & (((cop_state.saved_i2 >> 8) & 0x7F) | 0x80);
		if (vp < cop_state.vcmp)
			return;
	}
	coppperEnabledCurrLine = 1;
	copperPredict();
	if (!amigaEvents[AmigaEventCopper].active)
		amigaCpuSetSpcFlag(SpcFlagCopper);
}

void amigaCopperHandler() {
	amigaCpuSetSpcFlag(SpcFlagCopper);

	if (!coppperEnabledCurrLine)
		return;

	amigaEvents[AmigaEventCopper].active = 0;
}

void blitterDoneNotify() {
	if (cop_state.state != COP_bltwait)
		return;

	cop_state.hpos = currentHPos () & ~1;
	cop_state.vpos = vpos;
	cop_state.state = COP_wait;
	computeSpcFlagCopper();
}

void amigaDoCopper() {
	copperUpdate(currentHPos());
}

static inline void copperSyncWithCpu(int hpos, int doSchedule, u32 addr) {
	/* Need to let the copper advance to the current position.  */
	if (amigaEvents[AmigaEventCopper].active) {
		if (hpos != maxhpos) {
			/* There might be reasons why we don't actually need to bother
			   updating the copper.  */
			if (hpos < cop_state.first_sync)
				return;
			if ((cop_state.regtypes_modified & regtypes[addr & 0x1FE]) == 0)
				return;
		}
		amigaEvents[AmigaEventCopper].active = 0;
		if (doSchedule)
			amigaEventsSchedule();
		amigaCpuSetSpcFlag(SpcFlagCopper);
	}
	if (coppperEnabledCurrLine)
		copperUpdate(hpos);
}

static inline void do_sprites_1(int num, int cycle) {
	AmigaSprite *s = &spr[num];

	if (!cycle) {
		if (vpos == s->vstart)
			s->state = AmigaSprite::WaitingStop;
		if (vpos == s->vstop)
			s->state = AmigaSprite::Restart;
	}
	if (!dmaen(DMA_SPRITE))
		return;
	if (s->state == AmigaSprite::Restart || vpos == SpriteVblankEndline) {
		register u16 data = amigaMemChipGetWord(s->pt);
		s->pt += (sprite_width >> 3);
		if (cycle == 0) {
			SPRxPOS_1(data, num);
		} else {
			s->state = AmigaSprite::WaitingStart;
			SPRxCTL_1(data, num);
		}
	} else if (s->state == AmigaSprite::WaitingStop) {
		if (!cycle)
			SPRxDATA_1(amigaMemChipGetWord(s->pt), num);
		else
			SPRxDATB_1(amigaMemChipGetWord(s->pt), num);
		s->pt += 2;
		switch (sprite_width) {
		case 64: {
			u32 data32 = amigaMemChipGetWord(s->pt); s->pt+=2;
			u32 data641 = amigaMemChipGetWord(s->pt); s->pt+=2;
			u32 data642 = amigaMemChipGetWord(s->pt); s->pt+=2;
			if (cycle == 0) {
				sprdata[num][3] = data642;
				sprdata[num][2] = data641;
				sprdata[num][1] = data32;
			} else {
				sprdatb[num][3] = data642;
				sprdatb[num][2] = data641;
				sprdatb[num][1] = data32;
			}
			break;
		}
		case 32: {
			u32 data32 = amigaMemChipGetWord(s->pt); s->pt+=2;
			if (cycle == 0)
				sprdata[num][1] = data32;
			else
				sprdatb[num][1] = data32;
			break;
		}
		}
	}
}

#define SPR0_HPOS 0x15
static inline void do_sprites(int hpos) {
	/* I don't know whether this is right. Some programs write the sprite pointers
	 * directly at the start of the copper list. With the test ajainst currvp, the
	 * first two words of data are read on the second line in the frame. The problem
	 * occurs when the program jumps to another copperlist a few lines further down
	 * which _also_ writes the sprite pointer registers. This means that a) writing
	 * to the sprite pointers sets the state to SPR_restart; or b) that sprite DMA
	 * is disabled until the end of the vertical blanking interval. The HRM
	 * isn't clear - it says that the vertical sprite position can be set to any
	 * value, but this wouldn't be the first mistake... */
	/* Update: I modified one of the programs to write the sprite pointers the
	 * second time only _after_ the VBlank interval, and it showed the same behaviour
	 * as it did unmodified under UAE with the above check. This indicates that the
	 * solution below is correct. */
	/* Another update: seems like we have to use the NTSC value here (see Sanity Turmoil
	 * demo).  */
	/* Maximum for Sanity Turmoil: 27.
	   Minimum for Sanity Arte: 22.  */
	if (vpos < SpriteVblankEndline)
		return;

	int maxspr = hpos;
	int minspr = last_sprite_hpos;

	if (minspr >= SPR0_HPOS + MaxSprites * 4 || maxspr < SPR0_HPOS)
		return;

	if (maxspr > SPR0_HPOS + MaxSprites * 4)
		maxspr = SPR0_HPOS + MaxSprites * 4;
	if (minspr < SPR0_HPOS)
		minspr = SPR0_HPOS;
	if (!(minspr&1))
		minspr++;

	for (int i = minspr; i < maxspr; i+=2)
		do_sprites_1((i - SPR0_HPOS) / 4, ((i - SPR0_HPOS) & 3)>>1);
	last_sprite_hpos = hpos;
}

static inline void init_sprites() {
	for (int i = 0; i < MaxSprites; i++)
		spr[i].state = AmigaSprite::Restart;
	bzero(sprpos, sizeof sprpos);
	bzero(sprctl, sizeof sprctl);
}

static inline void init_hardware_frame() {
	next_lineno = 0;
	diwstate = DIW_waiting_start;
	hdiwstate = DIW_waiting_start;
}

void init_hardware_for_drawing_frame() {
	/* Avoid this code in the first frame after a amigaCustomReset.  */
	if (prev_sprite_entries) {
		int first_pixel = prev_sprite_entries[0].first_pixel;
		int npixels = prev_sprite_entries[prev_next_sprite_entry].first_pixel - first_pixel;
		bzero(spixels + first_pixel, npixels * sizeof *spixels);
		bzero(spixstate.bytes + first_pixel, npixels * sizeof *spixstate.bytes);
	}
	prev_next_sprite_entry = next_sprite_entry;

	next_color_change = 0;
	next_sprite_entry = 0;
	next_color_entry = 0;
	remembered_color_entry = -1;

	prev_sprite_entries = sprite_entries[current_change_set];
	curr_sprite_entries = sprite_entries[current_change_set ^ 1];
	curr_color_changes = color_changes[current_change_set ^ 1];
	curr_color_tables = color_tables[current_change_set ^ 1];
	current_change_set ^= 1;

	/* Use both halves of the array in alternating fashion.  */
	curr_sprite_entries[0].first_pixel = current_change_set * MAX_SPR_PIXELS;
	next_sprite_forced = 1;
}

static inline void vsyncHandler() {
	for (int i = 0; i < MaxSprites; i++)
		spr[i].state = AmigaSprite::WaitingStart;

	INTREQ (0x8020);

	if (bplcon0 & 4)
		lof ^= 0x8000;

	if (vsync_handler_cnt_disk_change == 0) {
		 amigaDiskUpdateReadyTime();
		 vsync_handler_cnt_disk_change = 5; //20;
	}
	vsync_handler_cnt_disk_change--;

	cop_state.ip = cop1lc;
	cop_state.state = COP_read1;
	cop_state.vpos = 0;
	cop_state.hpos = 0;
	cop_state.ignore_next = 0;

	init_hardware_frame();
	amigaCiaVSyncHandler();

	amigaEventVSync = true;
}

void amigaHSyncHandler() {
	/* Using 0x8A makes sure that we don't accidentally trip over the
	   modified_regtypes check.  */
	copperSyncWithCpu(maxhpos, 0, 0x8A);

	finish_decisions();

	amigaEvents[AmigaEventHSync].time += amigaCycles - lastHSyncCycles;
	lastHSyncCycles = amigaCycles;
	amigaCiaHSyncHandler();
	amigaSpuUpdate();
	amigaSpuFetchAudio();

	/* In theory only an equality test is needed here - but if a program
	   goes haywire with the VPOSW register, it can cause us to miss this,
	   with vpos going into the thousands (and all the nasty consequences
	   this has).  */

	vpos = (vpos+1) % 512;
	if (vpos >= (maxvpos + (lof != 0))) {
		vpos = 0;
		vsyncHandler();
	}

	amigaDiskUpdateHsync();

	if (amigaDrawEnabled) {
		next_lineno = vpos;
		reset_decisions();
	}
	/* See if there's a chance of a copper wait ending this line.  */
	cop_state.hpos = 0;
	computeSpcFlagCopper();
}

static void init_regtypes() {
	for (int i = 0; i < 512; i += 2) {
		regtypes[i] = REGTYPE_ALL;
		if ((i >= 0x20 && i < 0x28) || i == 0x08 || i == 0x7E)
			regtypes[i] = REGTYPE_DISK;
		else if (i >= 0x68 && i < 0x70)
			regtypes[i] = REGTYPE_NONE;
		else if (i >= 0x40 && i < 0x78)
			regtypes[i] = REGTYPE_BLITTER;
		else if (i >= 0xA0 && i < 0xE0 && (i & 0xF) < 0xE)
			regtypes[i] = REGTYPE_AUDIO;
		else if (i >= 0xA0 && i < 0xE0)
			regtypes[i] = REGTYPE_NONE;
		else if (i >= 0xE0 && i < 0x100)
			regtypes[i] = REGTYPE_PLANE;
		else if (i >= 0x120 && i < 0x180)
			regtypes[i] = REGTYPE_SPRITE;
		else if (i >= 0x180 && i < 0x1C0)
			regtypes[i] = REGTYPE_COLOR;
		else switch (i) {
		case 0x02:
			/* DMACONR - setting this to REGTYPE_BLITTER will cause it to
			   conflict with DMACON (since that is REGTYPE_ALL), and the
			   blitter registers (for the BBUSY bit), but nothing else,
			   which is (I think) what we want.  */
			regtypes[i] = REGTYPE_BLITTER;
			break;
		case 0x04: case 0x06: case 0x2A: case 0x2C:
			regtypes[i] = REGTYPE_POS;
			break;
		case 0x0A: case 0x0C:
		case 0x12: case 0x14: case 0x16:
		case 0x36:
			regtypes[i] = REGTYPE_JOYPORT;
			break;
		case 0x104:
		case 0x102:
			regtypes[i] = REGTYPE_PLANE;
			break;
		case 0x88: case 0x8A:
		case 0x8E: case 0x90: case 0x92: case 0x94:
		case 0x96:
		case 0x100:
			regtypes[i] |= REGTYPE_FORCE;
			break;
		}
	}
}

void amigaEventsInit() {
	amigaCycles = 0;
	lastHSyncCycles = 0;
	for (int i = 0; i < AmigaEventMax; i++)
		amigaEvents[i].active = 0;
	amigaEvents[AmigaEventHSync].time = maxhpos * CYCLE_UNIT + amigaCycles;
	amigaEvents[AmigaEventHSync].active = 1;
	amigaEventsSchedule();
}

void amigaCustomReset() {
	for (int i = 0; i < 32; i++) {
		currentColors.color_uae_regs_ecs[i] = 0;
		currentColors.acolors[i] = xcolors[0];
	}

	/* Clear the armed flags of all sprites.  */
	memset(spr, 0, sizeof spr);
	dmacon = intena = 0;

	copcon = 0;
	DSKLEN(0);

	bplcon0 = 0;
	bplcon0Planes = 0;
	bplcon0Resolution = 0;
	bplcon4 = 0x11; /* Get AJA chipset into ECS compatibility mode */
	bplcon3 = 0xC00;

	FMODE(0);
	CLXCON(0);
	lof = 0;

	vsync_handler_cnt_disk_change = 0;

	amigaDiskReset();
	amigaCiaReset();
	amigaCpuClearSpcFlag(~(SpcFlagBrk));
	vpos = 0;
	curr_sprite_entries = 0;
	prev_sprite_entries = 0;
	sprite_entries[0][0].first_pixel = 0;
	sprite_entries[1][0].first_pixel = MAX_SPR_PIXELS;
	sprite_entries[0][1].first_pixel = 0;
	sprite_entries[1][1].first_pixel = MAX_SPR_PIXELS;
	memset(spixels, 0, sizeof spixels);
	memset(&spixstate, 0, sizeof spixstate);
	bltstate = BLT_done;
	cop_state.state = COP_stop;
	diwstate = DIW_waiting_start;
	hdiwstate = DIW_waiting_start;
	amigaCycles = 0;
	init_hz(true);
	amigaSpuReset();
	init_sprites();
	init_hardware_frame();
	reset_decisions();
	init_regtypes();
	sprite_buffer_res = RES_LORES;
	expand_sprres();
}

static void genCustomTables() {
	for (int i = 0; i < 256; i++) {
		sprtaba[i] = ((((i >> 7) & 1) << 0)
				  | (((i >> 6) & 1) << 2)
				  | (((i >> 5) & 1) << 4)
				  | (((i >> 4) & 1) << 6)
				  | (((i >> 3) & 1) << 8)
				  | (((i >> 2) & 1) << 10)
				  | (((i >> 1) & 1) << 12)
				  | (((i >> 0) & 1) << 14));
		sprtabb[i] = sprtaba[i] << 1;
	}
}

void amigaCustomInit() {
	for (int num = 0; num < 2; num++) {
		sprite_entries[num] = new sprite_entry[MAX_SPRITE_ENTRY];
		color_changes[num] = new color_change[MAX_COLOR_CHANGE];
	}
	genCustomTables();
	build_blitfilltable();
	create_cycle_diagram_table();
}

void amigaCustomShutdown() {
	for (int num = 0; num < 2; num++) {
	   delete[] sprite_entries[num];
	   delete[] color_changes[num];
	}
}

/* Custom chip memory bank */

static u32 custom_lget(u32);
static u32 custom_wget(u32);
static u32 custom_bget(u32);
static void custom_lput(u32, u32);
static void custom_wput(u32, u32);
static void custom_bput(u32, u32);

static __inline__ u32 custom_wget_1(u32 addr) {
	u16 v;
	
	switch (addr & 0x1FE) {
	case 0x002: v = DMACONR (); break;
	case 0x004: v = VPOSR (); break;
	case 0x006: v = VHPOSR (); break;

	case 0x008: v = DSKDATR (currentHPos ()); break;

	case 0x00A: v = JOY0DAT (); break;
	case 0x00C: v =  JOY1DAT (); break;
	case 0x00E: v =  CLXDAT (); break;
	case 0x010: v = ADKCONR (); break;

	case 0x012: v = POT0DAT (); break;
	case 0x016: v = POTGOR (); break;
	case 0x018: v = 0; break;
	case 0x01A: v = DSKBYTR (currentHPos ()); break;
	case 0x01C: v = INTENAR (); break;
	case 0x01E: v = INTREQR (); break;
	case 0x07C: v = DENISEID (); break;

	case 0x180: case 0x182: case 0x184: case 0x186: case 0x188: case 0x18A:
	case 0x18C: case 0x18E: case 0x190: case 0x192: case 0x194: case 0x196:
	case 0x198: case 0x19A: case 0x19C: case 0x19E: case 0x1A0: case 0x1A2:
	case 0x1A4: case 0x1A6: case 0x1A8: case 0x1AA: case 0x1AC: case 0x1AE:
	case 0x1B0: case 0x1B2: case 0x1B4: case 0x1B6: case 0x1B8: case 0x1BA:
	case 0x1BC: case 0x1BE:
		v = COLOR_READ ((addr & 0x3E) / 2);
		break;

	default:
		v = last_custom_value;
		custom_wput (addr, v);
		last_custom_value = 0xFFFF;
		return v;
	}
	last_custom_value = v;
	return v;
}

u32 custom_wget(u32 addr) {
	copperSyncWithCpu(currentHPos(), 1, addr);
	return custom_wget_1(addr);
}

u32 custom_bget(u32 addr) {
	return custom_wget(addr & 0xFFFE) >> (addr & 1 ? 0 : 8);
}

u32 custom_lget(u32 addr) {
	return ((u32)custom_wget(addr & 0xFFFE) << 16) | custom_wget((addr + 2) & 0xFFFE);
}

void custom_wput_1(int hpos, u32 addr, u32 value) {
	addr &= 0x1FE;
	last_custom_value = value;
	switch (addr) {
	case 0x020: DSKPTH (value); break;
	case 0x022: DSKPTL (value); break;
	case 0x024: DSKLEN (value); break;
	case 0x026: DSKDAT (value); break;

	case 0x02A: VPOSW (value); break;
	case 0x02E: COPCON (value); break;
	case 0x030: break;
	case 0x032: break;
	case 0x034: POTGO (value); break;
	case 0x040: BLTCON0 (value); break;
	case 0x042: BLTCON1 (value); break;

	case 0x044: BLTAFWM (value); break;
	case 0x046: BLTALWM (value); break;

	case 0x050: BLTAPTH (value); break;
	case 0x052: BLTAPTL (value); break;
	case 0x04C: BLTBPTH (value); break;
	case 0x04E: BLTBPTL (value); break;
	case 0x048: BLTCPTH (value); break;
	case 0x04A: BLTCPTL (value); break;
	case 0x054: BLTDPTH (value); break;
	case 0x056: BLTDPTL (value); break;

	case 0x058: BLTSIZE (value); break;

	case 0x064: BLTAMOD (value); break;
	case 0x062: BLTBMOD (value); break;
	case 0x060: BLTCMOD (value); break;
	case 0x066: BLTDMOD (value); break;

	case 0x070: BLTCDAT (value); break;
	case 0x072: BLTBDAT (value); break;
	case 0x074: BLTADAT (value); break;

	case 0x07E: DSKSYNC (value); break;

	case 0x080: COP1LCH (value); break;
	case 0x082: COP1LCL (value); break;
	case 0x084: COP2LCH (value); break;
	case 0x086: COP2LCL (value); break;

	case 0x088: COPJMP1 (value); break;
	case 0x08A: COPJMP2 (value); break;

	case 0x08E: DIWSTRT (hpos, value); break;
	case 0x090: DIWSTOP (hpos, value); break;
	case 0x092: DDFSTRT (hpos, value); break;
	case 0x094: DDFSTOP (hpos, value); break;

	case 0x096: DMACON (hpos, value); break;
	case 0x098: CLXCON (value); break;
	case 0x09A: INTENA (value); break;
	case 0x09C: INTREQ (value); break;
	case 0x09E: ADKCON (value); break;

	case 0x0A0: AUDxLCH (0, value); break;
	case 0x0A2: AUDxLCL (0, value); break;
	case 0x0A4: AUDxLEN (0, value); break;
	case 0x0A6: AUDxPER (0, value); break;
	case 0x0A8: AUDxVOL (0, value); break;
	case 0x0AA: AUDxDAT (0, value); break;

	case 0x0B0: AUDxLCH (1, value); break;
	case 0x0B2: AUDxLCL (1, value); break;
	case 0x0B4: AUDxLEN (1, value); break;
	case 0x0B6: AUDxPER (1, value); break;
	case 0x0B8: AUDxVOL (1, value); break;
	case 0x0BA: AUDxDAT (1, value); break;

	case 0x0C0: AUDxLCH (2, value); break;
	case 0x0C2: AUDxLCL (2, value); break;
	case 0x0C4: AUDxLEN (2, value); break;
	case 0x0C6: AUDxPER (2, value); break;
	case 0x0C8: AUDxVOL (2, value); break;
	case 0x0CA: AUDxDAT (2, value); break;

	case 0x0D0: AUDxLCH (3, value); break;
	case 0x0D2: AUDxLCL (3, value); break;
	case 0x0D4: AUDxLEN (3, value); break;
	case 0x0D6: AUDxPER (3, value); break;
	case 0x0D8: AUDxVOL (3, value); break;
	case 0x0DA: AUDxDAT (3, value); break;

	case 0x0E0: BPLPTH (hpos, value, 0); break;
	case 0x0E2: BPLPTL (hpos, value, 0); break;
	case 0x0E4: BPLPTH (hpos, value, 1); break;
	case 0x0E6: BPLPTL (hpos, value, 1); break;
	case 0x0E8: BPLPTH (hpos, value, 2); break;
	case 0x0EA: BPLPTL (hpos, value, 2); break;
	case 0x0EC: BPLPTH (hpos, value, 3); break;
	case 0x0EE: BPLPTL (hpos, value, 3); break;
	case 0x0F0: BPLPTH (hpos, value, 4); break;
	case 0x0F2: BPLPTL (hpos, value, 4); break;
	case 0x0F4: BPLPTH (hpos, value, 5); break;
	case 0x0F6: BPLPTL (hpos, value, 5); break;
	case 0x0F8: BPLPTH (hpos, value, 6); break;
	case 0x0FA: BPLPTL (hpos, value, 6); break;
	case 0x0FC: BPLPTH (hpos, value, 7); break;
	case 0x0FE: BPLPTL (hpos, value, 7); break;

	case 0x100: BPLCON0 (hpos, value); break;
	case 0x102: BPLCON1 (hpos, value); break;
	case 0x104: BPLCON2 (hpos, value); break;
	case 0x106: BPLCON3 (hpos, value); break;

	case 0x108: BPL1MOD (hpos, value); break;
	case 0x10A: BPL2MOD (hpos, value); break;
	case 0x10E: CLXCON2 (value); break;

	case 0x110: BPL1DAT (hpos, value); break;
	case 0x112: BPL2DAT (value); break;
	case 0x114: BPL3DAT (value); break;
	case 0x116: BPL4DAT (value); break;
	case 0x118: BPL5DAT (value); break;
	case 0x11A: BPL6DAT (value); break;
	case 0x11C: BPL7DAT (value); break;
	case 0x11E: BPL8DAT (value); break;

	case 0x180: case 0x182: case 0x184: case 0x186: case 0x188: case 0x18A:
	case 0x18C: case 0x18E: case 0x190: case 0x192: case 0x194: case 0x196:
	case 0x198: case 0x19A: case 0x19C: case 0x19E: case 0x1A0: case 0x1A2:
	case 0x1A4: case 0x1A6: case 0x1A8: case 0x1AA: case 0x1AC: case 0x1AE:
	case 0x1B0: case 0x1B2: case 0x1B4: case 0x1B6: case 0x1B8: case 0x1BA:
	case 0x1BC: case 0x1BE:
		COLOR_WRITE (hpos, value & 0xFFF, (addr & 0x3E) / 2);
		break;
	case 0x120: case 0x124: case 0x128: case 0x12C:
	case 0x130: case 0x134: case 0x138: case 0x13C:
		SPRxPTH (hpos, value, (addr - 0x120) / 4);
		break;
	case 0x122: case 0x126: case 0x12A: case 0x12E:
	case 0x132: case 0x136: case 0x13A: case 0x13E:
		SPRxPTL (hpos, value, (addr - 0x122) / 4);
		break;
	case 0x140: case 0x148: case 0x150: case 0x158:
	case 0x160: case 0x168: case 0x170: case 0x178:
		SPRxPOS (hpos, value, (addr - 0x140) / 8);
		break;
	case 0x142: case 0x14A: case 0x152: case 0x15A:
	case 0x162: case 0x16A: case 0x172: case 0x17A:
		SPRxCTL (hpos, value, (addr - 0x142) / 8);
		break;
	case 0x144: case 0x14C: case 0x154: case 0x15C:
	case 0x164: case 0x16C: case 0x174: case 0x17C:
		SPRxDATA (hpos, value, (addr - 0x144) / 8);
		break;
	case 0x146: case 0x14E: case 0x156: case 0x15E:
	case 0x166: case 0x16E: case 0x176: case 0x17E:
		SPRxDATB (hpos, value, (addr - 0x146) / 8);
		break;

	case 0x36: JOYTEST (value); break;
	case 0x5A: BLTCON0L (value); break;
	case 0x5C: BLTSIZV (value); break;
	case 0x5E: BLTSIZH (value); break;
	case 0x1E4: DIWHIGH (hpos, value); break;
	case 0x10C: BPLCON4 (hpos, value); break;
	case 0x1FC: FMODE (value); break;
	}
}


void custom_wput(u32 addr, u32 value) {
	int hpos = currentHPos();
	copperSyncWithCpu(hpos, 1, addr);
	custom_wput_1(hpos, addr, value);
}

void custom_bput(u32 addr, u32 value) {
	/* Is this correct now? (There are people who bput things to the upper byte of AUDxVOL). */
	u16 rval = (value << 8) | (value & 0xFF);
	custom_wput(addr, rval);
}

void custom_lput(u32 addr, u32 value) {
	custom_wput(addr & 0xFFFE, value >> 16);
	custom_wput((addr + 2) & 0xFFFE, (u16)value);
}

AmigaAddrBank amigaCustomBank = {
	custom_lget, custom_wget, custom_bget,
	custom_lput, custom_wput, custom_bput,
	0
};

static void spriteSl(int i) {
	emsl.begin(QString("sprite%1").arg(i));
	emsl.var("pt", spr[i].pt);
	emsl.var("armed", spr[i].armed);
	emsl.var("pos", sprpos[i]);
	emsl.var("ctl", sprctl[i]);
	for (int j = 0; j < 4; j++) {
		emsl.var(QString("data%1").arg(j), sprdata[i][j]);
		emsl.var(QString("datb%1").arg(j), sprdatb[i][j]);
	}
	emsl.end();
}

void amigaCustomSl() {
	// events
	emsl.begin("events");
	emsl.var("cycles", amigaCycles);
	emsl.var("nextEvent", amigaNextEvent);
	emsl.var("lastHSyncCycles", lastHSyncCycles);
	emsl.end();

	for (int i = 0; i < AmigaEventMax; i++) {
		emsl.begin(QString("event%1").arg(i));
		emsl.var("active", amigaEvents[i].active);
		emsl.var("time", amigaEvents[i].time);
		emsl.end();
	}
	// custom
	emsl.begin("custom");
	if (!emsl.save) {
		if (amigaEvents[AmigaEventBlitter].active) {
			uint olddmacon = dmacon;
			dmacon |= DMA_BLITTER; /* ugh.. */
			amigaBlitterHandler();
			dmacon = olddmacon;
		}
	}
	emsl.var("dmacon", dmacon);
	emsl.var("copcon", copcon);
	emsl.var("potgo", potgo);
	emsl.var("lof", lof);

	emsl.var("bltcon0", bltcon0);
	emsl.var("bltcon1", bltcon1);

	emsl.var("bltafwm", blt_info.bltafwm);
	emsl.var("bltalwm", blt_info.bltalwm);

	emsl.var("bltcpt", bltcpt);
	emsl.var("bltbpt", bltbpt);
	emsl.var("bltapt", bltapt);
	emsl.var("bltdpt", bltdpt);

	emsl.var("bltcmod", blt_info.bltcmod);
	emsl.var("bltbmod", blt_info.bltbmod);
	emsl.var("bltamod", blt_info.bltamod);
	emsl.var("bltdmod", blt_info.bltdmod);

	emsl.var("bltcdat", blt_info.bltcdat);
	emsl.var("bltbdat", blt_info.bltbdat);
	emsl.var("bltadat", blt_info.bltadat);

	emsl.var("cop1lc", cop1lc);
	emsl.var("cop2lc", cop2lc);

	emsl.var("diwstrt", diwstrt);
	emsl.var("diwstop", diwstop);
	emsl.var("ddfstrt", ddfstrt);
	emsl.var("ddfstop", ddfstop);

	emsl.var("intena", intena);
	emsl.var("intreq", intreq);
	emsl.var("adkcon", adkcon);
	SET_INTERRUPT();

	for (int i = 0; i < 8; i++)
		emsl.var(QString("bplpt%1").arg(i), bplpt[i]);

	emsl.var("bplcon0", bplcon0);
	emsl.var("bplcon1", bplcon1);
	emsl.var("bplcon2", bplcon2);
	emsl.var("bplcon3", bplcon3);
	emsl.var("bplcon4", bplcon4);

	emsl.var("bpl1mod", bpl1mod);
	emsl.var("bpl2mod", bpl2mod);

	emsl.array("colors", &currentColors, sizeof(currentColors));
	emsl.var("last_custom_value", last_custom_value);

	if (!emsl.save) {
		COPCON(copcon);
		POTGO(potgo);

		BLTCON0(bltcon0);
		BLTCON1(bltcon1);

		BLTAFWM(blt_info.bltafwm);
		BLTALWM(blt_info.bltalwm);

		BLTCMOD(blt_info.bltcmod);
		BLTBMOD(blt_info.bltbmod);
		BLTAMOD(blt_info.bltamod);
		BLTDMOD(blt_info.bltdmod);

		BLTCDAT(blt_info.bltcdat);
		BLTBDAT(blt_info.bltbdat);
		BLTADAT(blt_info.bltadat);

		COPJMP1(0);

		int savedbplcon0 = bplcon0;
		BPLCON0(0, 0);
		BPLCON0(0, savedbplcon0);

		for (int i = 0 ; i < 32 ; i++) {
			u32 vv = currentColors.color_uae_regs_ecs[i];
			currentColors.color_uae_regs_ecs[i] = (unsigned)-1;
			record_color_change (0, i, vv);
			remembered_color_entry = -1;
			currentColors.color_uae_regs_ecs[i] = vv;
			currentColors.acolors[i] = xcolors[vv];
		}
		calcdiw();
	}
	emsl.end();

	for (int i = 0; i < MaxSprites; i++)
		spriteSl(i);
}
