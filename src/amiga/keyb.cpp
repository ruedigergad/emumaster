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

#include "cia.h"

#define AK_A 0x20
#define AK_B 0x35
#define AK_C 0x33
#define AK_D 0x22
#define AK_E 0x12
#define AK_F 0x23
#define AK_G 0x24
#define AK_H 0x25
#define AK_I 0x17
#define AK_J 0x26
#define AK_K 0x27
#define AK_L 0x28
#define AK_M 0x37
#define AK_N 0x36
#define AK_O 0x18
#define AK_P 0x19
#define AK_Q 0x10
#define AK_R 0x13
#define AK_S 0x21
#define AK_T 0x14
#define AK_U 0x16
#define AK_V 0x34
#define AK_W 0x11
#define AK_X 0x32
#define AK_Y 0x15
#define AK_Z 0x31

#define AK_0 0x0A
#define AK_1 0x01
#define AK_2 0x02
#define AK_3 0x03
#define AK_4 0x04
#define AK_5 0x05
#define AK_6 0x06
#define AK_7 0x07
#define AK_8 0x08
#define AK_9 0x09

#define AK_NP0 0x0F
#define AK_NP1 0x1D
#define AK_NP2 0x1E
#define AK_NP3 0x1F
#define AK_NP4 0x2D
#define AK_NP5 0x2E
#define AK_NP6 0x2F
#define AK_NP7 0x3D
#define AK_NP8 0x3E
#define AK_NP9 0x3F

#define AK_NPDIV 0x5C
#define AK_NPMUL 0x5D
#define AK_NPSUB 0x4A
#define AK_NPADD 0x5E
#define AK_NPDEL 0x3C
#define AK_NPLPAREN 0x5A
#define AK_NPRPAREN 0x5B

#define AK_F1 0x50
#define AK_F2 0x51
#define AK_F3 0x52
#define AK_F4 0x53
#define AK_F5 0x54
#define AK_F6 0x55
#define AK_F7 0x56
#define AK_F8 0x57
#define AK_F9 0x58
#define AK_F10 0x59

#define AK_UP 0x4C
#define AK_DN 0x4D
#define AK_LF 0x4F
#define AK_RT 0x4E

#define AK_SPC 0x40
#define AK_BS 0x41
#define AK_TAB 0x42
#define AK_ENT 0x43
#define AK_RET 0x44
#define AK_ESC 0x45
#define AK_DEL 0x46

#define AK_LSH 0x60
#define AK_RSH 0x61
#define AK_CAPSLOCK 0x62
#define AK_CTRL 0x63
#define AK_LALT 0x64
#define AK_RALT 0x65
#define AK_LAMI 0x66
#define AK_RAMI 0x67
#define AK_HELP 0x5F

/* The following have different mappings on national keyboards */

#define AK_LBRACKET 0x1A
#define AK_RBRACKET 0x1B
#define AK_SEMICOLON 0x29
#define AK_COMMA 0x38
#define AK_PERIOD 0x39
#define AK_SLASH 0x3A
#define AK_BACKSLASH 0x0D
#define AK_QUOTE 0x2A
#define AK_NUMBERSIGN 0x2B
#define AK_LTGT 0x30
#define AK_BACKQUOTE 0x00
#define AK_MINUS 0x0B
#define AK_EQUAL 0x0C

static int translateKey(int hostKey) {
	switch (hostKey) {
	case Qt::Key_A: return AK_A;
	case Qt::Key_B: return AK_B;
	case Qt::Key_C: return AK_C;
	case Qt::Key_D: return AK_D;
	case Qt::Key_E: return AK_E;
	case Qt::Key_F: return AK_F;
	case Qt::Key_G: return AK_G;
	case Qt::Key_H: return AK_H;
	case Qt::Key_I: return AK_I;
	case Qt::Key_J: return AK_J;
	case Qt::Key_K: return AK_K;
	case Qt::Key_L: return AK_L;
	case Qt::Key_M: return AK_M;
	case Qt::Key_N: return AK_N;
	case Qt::Key_O: return AK_O;
	case Qt::Key_P: return AK_P;
	case Qt::Key_Q: return AK_Q;
	case Qt::Key_R: return AK_R;
	case Qt::Key_S: return AK_S;
	case Qt::Key_T: return AK_T;
	case Qt::Key_U: return AK_U;
	case Qt::Key_W: return AK_W;
	case Qt::Key_V: return AK_V;
	case Qt::Key_X: return AK_X;
	case Qt::Key_Y: return AK_Y;
	case Qt::Key_Z: return AK_Z;

	case Qt::Key_0: return AK_0;
	case Qt::Key_1: return AK_1;
	case Qt::Key_2: return AK_2;
	case Qt::Key_3: return AK_3;
	case Qt::Key_4: return AK_4;
	case Qt::Key_5: return AK_5;
	case Qt::Key_6: return AK_6;
	case Qt::Key_7: return AK_7;
	case Qt::Key_8: return AK_8;
	case Qt::Key_9: return AK_9;

//	case Qt::Key_KP0: return AK_NP0;
//	case Qt::Key_KP1: return AK_NP1;
//	case Qt::Key_KP2: return AK_NP2;
//	case Qt::Key_KP3: return AK_NP3;
//	case Qt::Key_KP4: return AK_NP4;
//	case Qt::Key_KP5: return AK_NP5;
//	case Qt::Key_KP6: return AK_NP6;
//	case Qt::Key_KP7: return AK_NP7;
//	case Qt::Key_KP8: return AK_NP8;
//	case Qt::Key_KP9: return AK_NP9;
//	case Qt::Key_KP_DIVIDE: return AK_NPDIV;
//	case Qt::Key_KP_MULTIPLY: return AK_NPMUL;
//	case Qt::Key_KP_MINUS: return AK_NPSUB;
//	case Qt::Key_KP_PLUS: return AK_NPADD;
//	case Qt::Key_KP_PERIOD: return AK_NPDEL;
//	case Qt::Key_KP_ENTER: return AK_ENT;

	case Qt::Key_F1: return AK_F1;
	case Qt::Key_F2: return AK_F2;
	case Qt::Key_F3: return AK_F3;
	case Qt::Key_F4: return AK_F4;
	case Qt::Key_F5: return AK_F5;
	case Qt::Key_F6: return AK_F6;
	case Qt::Key_F7: return AK_F7;
	case Qt::Key_F8: return AK_F8;
	case Qt::Key_F9: return AK_F9;
	case Qt::Key_F10: return AK_F10;

	case Qt::Key_Backspace: return AK_BS;
	case Qt::Key_Delete: return AK_DEL;
	case Qt::Key_Control: return AK_CTRL;
	case Qt::Key_Tab: return AK_TAB;
	case Qt::Key_Alt: return AK_LALT;
	case Qt::Key_AltGr: return AK_RALT;
	case Qt::Key_Meta: return AK_RAMI;
//	case Qt::Key_LMETA: return AK_LAMI;
	case Qt::Key_Return: return AK_RET;
	case Qt::Key_Space: return AK_SPC;
	case Qt::Key_Shift: return AK_LSH;
//	case Qt::Key_RSHIFT: return AK_RSH;
	case Qt::Key_Escape: return AK_ESC;

	case Qt::Key_Insert: return AK_HELP;
	case Qt::Key_Home: return AK_NPLPAREN;
	case Qt::Key_End: return AK_NPRPAREN;
	case Qt::Key_CapsLock: return AK_CAPSLOCK;

	case Qt::Key_Up: return AK_UP;
	case Qt::Key_Down: return AK_DN;
	case Qt::Key_Left: return AK_LF;
	case Qt::Key_Right: return AK_RT;

	case Qt::Key_PageUp: return AK_RAMI;          // PgUp mapped to right amiga
	case Qt::Key_PageDown: return AK_LAMI;        // PgDn mapped to left amiga

	case Qt::Key_BraceLeft: return AK_LBRACKET;
	case Qt::Key_BracketRight: return AK_RBRACKET;
	case Qt::Key_Comma: return AK_COMMA;
	case Qt::Key_Period: return AK_PERIOD;
	case Qt::Key_Slash: return AK_SLASH;
	case Qt::Key_Semicolon: return AK_SEMICOLON;
	case Qt::Key_Minus: return AK_MINUS;
	case Qt::Key_Equal: return AK_EQUAL;
	case Qt::Key_QuoteLeft: return AK_QUOTE;
	case Qt::Key_Apostrophe: return AK_BACKQUOTE;
	case Qt::Key_Backslash: return AK_BACKSLASH;

	default: break;
	}
	return -1;
}

void amigaRecordKey(int hostKey, bool down) {
	int amigaKey = translateKey(hostKey);
	if (amigaKey < 0)
		return;
	amigaKey <<= 1;
	if (!down)
		amigaKey |= 1;
	amigaKeys.append(amigaKey);
}
