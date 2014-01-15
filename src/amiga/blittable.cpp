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

#include "custom.h"
#include "mem.h"
#include "blitter.h"
#include "blitfunc.h"

blitter_func *blitfunc_dofast[256] = {
blitdofast_0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_2a, 0, 0, 0, 0, 0, 
blitdofast_30, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_3a, 0, blitdofast_3c, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_4a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_6a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_8a, 0, blitdofast_8c, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_9a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
blitdofast_a8, 0, blitdofast_aa, 0, 0, 0, 0, 0, 
0, blitdofast_b1, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_ca, 0, blitdofast_cc, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
blitdofast_d8, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_e2, 0, 0, 0, 0, 0, 
0, 0, blitdofast_ea, 0, 0, 0, 0, 0, 
blitdofast_f0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_fa, 0, blitdofast_fc, 0, 0, 0
};

blitter_func *blitfunc_dofast_desc[256] = {
blitdofast_desc_0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_2a, 0, 0, 0, 0, 0, 
blitdofast_desc_30, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_3a, 0, blitdofast_desc_3c, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_4a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_6a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_8a, 0, blitdofast_desc_8c, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_9a, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
blitdofast_desc_a8, 0, blitdofast_desc_aa, 0, 0, 0, 0, 0, 
0, blitdofast_desc_b1, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_ca, 0, blitdofast_desc_cc, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
blitdofast_desc_d8, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_e2, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_ea, 0, 0, 0, 0, 0, 
blitdofast_desc_f0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, blitdofast_desc_fa, 0, blitdofast_desc_fc, 0, 0, 0
};
