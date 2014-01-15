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

#ifndef NESDISASSEMBLER_H
#define NESDISASSEMBLER_H

#include <base/emu.h>

class NesInstruction
{
public:
	const char *disassembled;
	const char *comment;
	u8 op;
	u8 pad1;
	u16 pad2;
	u32 pad3;
};

class NesDisassembler
{
public:
	NesDisassembler(u8 *nesMemory);
	void clear();
	QList<u16> positions() const;
	u16 nextInstruction(u16 pc) const;
	NesInstruction instruction(u16 pc) const;
};

#endif // NESDISASSEMBLER_H
