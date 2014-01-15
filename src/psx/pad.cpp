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

#include "psx.h"

// TODO multiplayer

// MOUSE SCPH-1030
#define PSE_PAD_TYPE_MOUSE			1
// NEGCON - 16 button analog controller SLPH-00001
#define PSE_PAD_TYPE_NEGCON			2
// GUN CONTROLLER - gun controller SLPH-00014 from Konami
#define PSE_PAD_TYPE_GUN			3
// STANDARD PAD SCPH-1080, SCPH-1150
#define PSE_PAD_TYPE_STANDARD		4
// ANALOG JOYSTICK SCPH-1110
#define PSE_PAD_TYPE_ANALOGJOY		5
// GUNCON - gun controller SLPH-00034 from Namco
#define PSE_PAD_TYPE_GUNCON			6
// ANALOG CONTROLLER SCPH-1150
#define PSE_PAD_TYPE_ANALOGPAD		7

static const int mapping[16] = {
	EmuPad::Button_Select,
	0,
	0,
	EmuPad::Button_Start,
	EmuPad::Button_Up,
	EmuPad::Button_Right,
	EmuPad::Button_Down,
	EmuPad::Button_Left,
	EmuPad::Button_L2,
	EmuPad::Button_R2,
	EmuPad::Button_L1,
	EmuPad::Button_R1,
	EmuPad::Button_X,
	EmuPad::Button_A,
	EmuPad::Button_B,
	EmuPad::Button_Y
};

static int buttonStatus = 0xFFFF;

void setPadButtons(int emuKeys) {
	buttonStatus = 0xFFFF;
	for (int i = 0; i < 16; i++) {
		if (emuKeys & mapping[i])
			buttonStatus &= ~(1 << i);
	}
}

static u8 *buf;

u8 pad2StartPoll(u8 data) {
	Q_UNUSED(data);
	return 0xFF;
}

u8 pad2Poll(u8 data) {
	Q_UNUSED(data);
	return 0xFF;
}

enum {
	CMD_READ_DATA_AND_VIBRATE = 0x42,
	CMD_CONFIG_MODE = 0x43,
	CMD_SET_MODE_AND_LOCK = 0x44,
	CMD_QUERY_MODEL_AND_MODE = 0x45,
	CMD_QUERY_ACT = 0x46, // ??
	CMD_QUERY_COMB = 0x47, // ??
	CMD_QUERY_MODE = 0x4C, // QUERY_MODE ??
	CMD_VIBRATION_TOGGLE = 0x4D
};

static u8 stdpar[2][8] = {
	{0xFF, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80},
	{0xFF, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80}
};

static u8 unk46[2][8] = {
	{0xFF, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
	{0xFF, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A}
};

static u8 unk47[2][8] = {
	{0xFF, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00}
};

static u8 unk4c[2][8] = {
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static u8 unk4d[2][8] = {
	{0xFF, 0x5A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{0xFF, 0x5A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};

static u8 stdcfg[2][8]   = {
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static u8 stdmode[2][8]  = {
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xFF, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static u8 stdmodel[2][8] = {
	{0xFF,
	 0x5A,
	 0x01, // 03 - dualshock2, 01 - dualshock
	 0x02, // number of modes
	 0x01, // current mode: 01 - analog, 00 - digital
	 0x02,
	 0x01,
	 0x00},
	{0xFF,
	 0x5A,
	 0x01, // 03 - dualshock2, 01 - dualshock
	 0x02, // number of modes
	 0x01, // current mode: 01 - analog, 00 - digital
	 0x02,
	 0x01,
	 0x00}
};

static u8 byteIndex = 0, command = 0, cmdLength = 0;

static u8 do_cmd(void) {
	cmdLength = 8;
	switch (command) {
		case CMD_SET_MODE_AND_LOCK:
			buf = stdmode[0];
			return 0xF3;

		case CMD_QUERY_MODEL_AND_MODE:
			buf = stdmodel[0];
			buf[4] = 0;//padstate[0].PadMode;
			return 0xF3;

		case CMD_QUERY_ACT:
			buf = unk46[0];
			return 0xF3;

		case CMD_QUERY_COMB:
			buf = unk47[0];
			return 0xF3;

		case CMD_QUERY_MODE:
			buf = unk4c[0];
			return 0xF3;

		case CMD_VIBRATION_TOGGLE:
			buf = unk4d[0];
			return 0xF3;

		case CMD_CONFIG_MODE:
//			if (padstate[0].ConfigMode) {
//				buf = stdcfg[0];
//				return 0xF3;
//			}
			// else FALLTHROUGH

		case CMD_READ_DATA_AND_VIBRATE:
		default:
			buf = stdpar[0];

			buf[2] = buttonStatus;
			buf[3] = buttonStatus >> 8;

//			if (padstate[0].PadMode == 1) {
//				buf[4] = rightJoyX;
//				buf[5] = rightJoyY;
//				buf[6] = leftJoyX;
//				buf[7] = leftJoyY;
//			} else {
				cmdLength = 4;
//			}

			return 0x41;//padstate[0].PadID;
	}
}

static void do_cmd2(u8 data)
{
	switch (command) {
		case CMD_CONFIG_MODE:
//			padstate[0].ConfigMode = data;
			break;

		case CMD_SET_MODE_AND_LOCK:
//			padstate[0].PadMode = data;
//			padstate[0].PadID = data ? 0x73 : 0x41;
			break;

		case CMD_QUERY_ACT:
			switch (data) {
				case 0: // default
					buf[5] = 0x02;
					buf[6] = 0x00;
					buf[7] = 0x0A;
					break;

				case 1: // Param std conf change
					buf[5] = 0x01;
					buf[6] = 0x01;
					buf[7] = 0x14;
					break;
			}
			break;

		case CMD_QUERY_MODE:
			switch (data) {
				case 0: // mode 0 - digital mode
					buf[5] = PSE_PAD_TYPE_STANDARD;
					break;

				case 1: // mode 1 - analog mode
					buf[5] = PSE_PAD_TYPE_ANALOGPAD;
					break;
			}
			break;
	}
}

u8 pad1StartPoll(u8 data) {
	Q_UNUSED(data);
	byteIndex = 0;
	return 0xFF;
}

u8 pad1Poll(u8 data) {
	if (byteIndex == 0) {
		command = data;
		byteIndex++;

		// Don't enable Analog/Vibration for a standard pad
//		if (padstate[0].pad.controllerType != PSE_PAD_TYPE_ANALOGPAD)
			command = CMD_READ_DATA_AND_VIBRATE;

		return do_cmd();
	}

	if (byteIndex == 2)
		do_cmd2(data);

	if (byteIndex >= cmdLength)
		return 0;

	return buf[byteIndex++];
}
