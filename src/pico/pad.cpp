/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	(c) Copyright 2011, elemental
*/

#include "pico.h"

static const int buttonsMapping[] = {
	EmuPad::Button_Up,
	EmuPad::Button_Down,
	EmuPad::Button_Left,
	EmuPad::Button_Right,
	EmuPad::Button_B,
	EmuPad::Button_X,
	EmuPad::Button_A,
	EmuPad::Button_Start,
	EmuPad::Button_R1,
	EmuPad::Button_L1,
	EmuPad::Button_Y,
	EmuPad::Button_Select
};

void PicoEmu::updateInput() {
	for (int padIndex = 0; padIndex < 2; padIndex++) {
		int buttons = 0;
		int hostButtons = input()->pad[padIndex].buttons();

		for (uint i = 0; i < sizeof(buttonsMapping); i++) {
			if (hostButtons & buttonsMapping[i])
				buttons |= 1 << i;
		}

		PicoPad[padIndex] = buttons;
	}
}
