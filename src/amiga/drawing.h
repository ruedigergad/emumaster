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

#ifndef AMIGADRAW_H
#define AMIGADRAW_H

#include <base/emu.h>

#define MAX_PLANES 6

#define RES_LORES 0
#define RES_HIRES 1
#define RES_SUPERHIRES 2

/* According to the HRM, pixel data spends a couple of cycles somewhere in the chips
   before it appears on-screen.  */
#define DIW_DDF_OFFSET 9

/* We ignore that many lores pixels at the start of the display. These are
 * invisible anyway due to hardware DDF limits. */
#define DISPLAY_LEFT_SHIFT 0x40
#define PIXEL_XPOS(HPOS) ((((HPOS)<<1) - DISPLAY_LEFT_SHIFT + DIW_DDF_OFFSET - 1) )

#define max_diwlastword (PIXEL_XPOS(maxhpos) - 6)

/* 440 rather than 880, since sprites are always lores.  */
#define MAX_PIXELS_PER_LINE 1760

/* No divisors for MAX_PIXELS_PER_LINE; we support AJA and may one day
   want to use SHRES sprites.  */
#define MAX_SPR_PIXELS (((MAXVPOS + 1)*2 + 1) * MAX_PIXELS_PER_LINE)

/* color values in two formats: 12 (OCS/ECS) or 24 (AJA) bit Amiga RGB (color_uae_regs),
 * and the native color value; both for each Amiga hardware color register.
 *
 * !!! See color_reg_xxx functions below before touching !!!
 */
struct color_entry {
	u16 color_uae_regs_ecs[32];
	uint acolors[256];
};

/* struct decision contains things we save across drawing frames for
 * comparison (smart update stuff). */
struct decision {
	/* Records the leftmost access of BPL1DAT.  */
	int plfleft, plfright, plflinelen;
	/* Display window: native coordinates, depend on lores state.  */
	int diwfirstword, diwlastword;
	int ctable;

	u16 bplcon0, bplcon2;
	u16 bplcon3, bplcon4;
	u8 nr_planes;
	u8 bplres;
};

/*
 * The idea behind this code is that at some point during each horizontal
 * line, we decide how to draw this line. There are many more-or-less
 * independent decisions, each of which can be taken at a different horizontal
 * position.
 * Sprites and color changes are handled specially: There isn't a single decision,
 * but a list of structures containing information on how to draw the line.
 */

struct color_change {
	int linepos;
	int regno;
	unsigned long value;
};

/* Anything related to changes in hw registers during the DDF for one
 * line. */
struct draw_info {
	int first_sprite_entry, last_sprite_entry;
	int first_color_change, last_color_change;
	int nr_color_changes, nr_sprites;
};

struct sprite_entry {
	u16 pos;
	u16 max;
	u32 first_pixel;
	u32 has_attached;
};

union sps_union {
	u8 bytes[2 * MAX_SPR_PIXELS];
	u32 words[2 * MAX_SPR_PIXELS / 4];
};

extern uint xcolors[4096];
extern bool amigaDrawEnabled;

extern union sps_union spixstate;
extern u16 spixels[MAX_SPR_PIXELS * 2];

extern struct color_change *color_changes[2];

extern struct color_entry color_tables[2][(MAXVPOS+1) * 2];
extern struct color_entry *curr_color_tables;

extern struct sprite_entry *curr_sprite_entries, *prev_sprite_entries;
extern struct color_change *curr_color_changes;
extern struct draw_info curr_drawinfo[];

extern struct decision line_decisions[2 * (MAXVPOS+1) + 1];
extern u8 line_data[(MAXVPOS+1) * 2][MAX_PLANES * MAX_WORDS_PER_LINE * 2];

/* Finally, stuff that shouldn't really be shared.  */

extern int diwfirstword,diwlastword;

extern void amigaDrawInit(char *frameBits, int framePitch);

extern void vSyncHandleRedraw();
extern void init_hardware_for_drawing_frame();

static __inline__ int coord_hw_to_window_x(int x)
{ return x - DISPLAY_LEFT_SHIFT; }

static __inline__ int coord_diw_to_window_x(int x)
{ return (x - DISPLAY_LEFT_SHIFT + DIW_DDF_OFFSET - 1); }

static __inline__ int color_reg_get(struct color_entry *ce, int c)
{ return ce->color_uae_regs_ecs[c]; }

static __inline__ void color_reg_set(struct color_entry *ce, int c, int v)
{ ce->color_uae_regs_ecs[c] = v; }

/* ugly copy hack, is there better solution? */
static __inline__ void color_reg_cpy(struct color_entry *dst, struct color_entry *src) {
	/* copy first 32 acolors and color_uae_regs_ecs */
	memcpy(dst->color_uae_regs_ecs, src->color_uae_regs_ecs,
		sizeof(struct color_entry) - sizeof(uint) * (256-32));
}

#endif // AMIGADRAW_H
