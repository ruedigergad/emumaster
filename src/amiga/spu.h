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

#ifndef AMIGASPU_H
#define AMIGASPU_H

#include <base/emu.h>

extern void AUDxDAT(int nr, u16 value);
extern void AUDxVOL(int nr, u16 value);
extern void AUDxPER(int nr, u16 value);
extern void AUDxLCH(int nr, u16 value);
extern void AUDxLCL(int nr, u16 value);
extern void AUDxLEN(int nr, u16 value);

extern void amigaSpuReset();
extern void amigaSpuUpdate();
extern void amigaSpuSchedule();
extern void amigaSpuHandler();
extern void amigaSpuCheckDma();
extern void amigaSpuFetchAudio();
extern void amigaSpuUpdateAdkMasks();
extern void amigaSpuDefaultEvtime();
extern int amigaSpuFillAudioBuffer(char *stream, int length);
extern void amigaSpuSl();

#endif // AMIGASPU_H
