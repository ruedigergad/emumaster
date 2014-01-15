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

#include "disassembler.h"
#include <stdio.h>

class MemoryComment
{
public:
	u16 begin;
	u16 end;
	const char *comment;
};

class Dis6502Str
{
public:
	const char *nimonic;
	int type;
};

// dis6502str.type = AddressType | AccessType;

enum AddressType {
	ADT_NONE = 0,
	ADT_IMM,				//
	ADT_AREG,				// A Register
	ADT_ZEROP,				// Zero Page
	ADT_ZEROPX,				// Zero Page,X
	ADT_ZEROPY,				// Zero Page,Y
	ADT_REL,				//
	ADT_ABS,				//
	ADT_ABSX,				//
	ADT_ABSY,				//
	ADT_ABSIND,				//
	ADT_PREIND,				// (Zero Page,X)
	ADT_POSTIND,			// (Zero Page),Y
	ADT_MASK = 0xff
};

enum AccessType {
	ACT_NL		= 0x000,
	ACT_RD		= 0x100,
	ACT_WT		= 0x200,
	ACT_WD		= 0x400,
	ACT_RW		= ACT_RD|ACT_WT,
	ACT_MASK	= ACT_RW
};

enum NameType {
	NMT_UND = 0,
	NMT_ADC,
	NMT_AND,
	NMT_ASL,
	NMT_BCC,
	NMT_BCS,
	NMT_BEQ,
	NMT_BIT,
	NMT_BMI,
	NMT_BNE,
	NMT_BPL,
	NMT_BRK,
	NMT_BVC,
	NMT_BVS,
	NMT_CLC,
	NMT_CLD,
	NMT_CLI,
	NMT_CLV,
	NMT_CMP,
	NMT_CPX,
	NMT_CPY,
	NMT_DEC,
	NMT_DEX,
	NMT_DEY,
	NMT_EOR,
	NMT_INC,
	NMT_INX,
	NMT_INY,
	NMT_JMP,
	NMT_JSR,
	NMT_LDA,
	NMT_LDX,
	NMT_LDY,
	NMT_LSR,
	NMT_NOP,
	NMT_ORA,
	NMT_PHA,
	NMT_PHP,
	NMT_PLA,
	NMT_PLP,
	NMT_ROL,
	NMT_ROR,
	NMT_RTI,
	NMT_RTS,
	NMT_SBC,
	NMT_SEC,
	NMT_SED,
	NMT_SEI,
	NMT_STA,
	NMT_STX,
	NMT_STY,
	NMT_TAX,
	NMT_TAY,
	NMT_TSX,
	NMT_TXA,
	NMT_TXS,
	NMT_TYA
};

const char *adtString[] = {
	"",
	"#$%02x",
	"a",
	"$%02x",
	"$%02x,x",
	"$%02x,y",
	"$%04x",
	"$%04x",
	"$%04x,x",
	"$%04x,y",
	"($%04x)",
	"($%02x,x)",
	"($%02x),y",
};

int opByteLength[] = {
	1,2,1,2,2,2,2,3,3,3,3,2,2
};

const char *opname[] = {
	"db",
	"adc",
	"and",
	"asl",
	"bcc",
	"bcs",
	"beq",
	"bit",
	"bmi",
	"bne",
	"bpl",
	"brk",
	"bvc",
	"bvs",
	"clc",
	"cld",
	"cli",
	"clv",
	"cmp",
	"cpx",
	"cpy",
	"dec",
	"dex",
	"dey",
	"eor",
	"inc",
	"inx",
	"iny",
	"jmp",
	"jsr",
	"lda",
	"ldx",
	"ldy",
	"lsr",
	"nop",
	"ora",
	"pha",
	"php",
	"pla",
	"plp",
	"rol",
	"ror",
	"rti",
	"rts",
	"sbc",
	"sec",
	"sed",
	"sei",
	"sta",
	"stx",
	"sty",
	"tax",
	"tay",
	"tsx",
	"txa",
	"txs",
	"tya",
};

Dis6502Str dis6502[256] = {
	// 00-0f
	{ opname[NMT_BRK],ACT_NL | ADT_NONE },
	{ opname[NMT_ORA],ACT_RD | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ORA],ACT_RD | ADT_ZEROP },
	{ opname[NMT_ASL],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_PHP],ACT_WT | ADT_NONE },
	{ opname[NMT_ORA],ACT_NL | ADT_IMM },
	{ opname[NMT_ASL],ACT_NL | ADT_AREG },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ORA],ACT_RD | ADT_ABS },
	{ opname[NMT_ASL],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 10-1f
	{ opname[NMT_BPL],ACT_NL | ADT_REL },
	{ opname[NMT_ORA],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ORA],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_ASL],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CLC],ACT_NL | ADT_NONE },
	{ opname[NMT_ORA],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ORA],ACT_RD | ADT_ABSX },
	{ opname[NMT_ASL],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 20-2f
	{ opname[NMT_JSR],ACT_NL | ADT_ABS },
	{ opname[NMT_AND],ACT_RD | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_BIT],ACT_RD | ADT_ZEROP },
	{ opname[NMT_AND],ACT_RD | ADT_ZEROP },
	{ opname[NMT_ROL],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_RD | ADT_NONE },
	{ opname[NMT_PLP],ACT_RD | ADT_NONE },
	{ opname[NMT_AND],ACT_RD | ADT_IMM },
	{ opname[NMT_ROL],ACT_NL | ADT_AREG },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_BIT],ACT_RD | ADT_ABS },
	{ opname[NMT_AND],ACT_RD | ADT_ABS },
	{ opname[NMT_ROL],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 30-3f
	{ opname[NMT_BMI],ACT_NL | ADT_REL },
	{ opname[NMT_AND],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_AND],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_ROL],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_SEC],ACT_NL | ADT_NONE },
	{ opname[NMT_AND],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_AND],ACT_RD | ADT_ABSX },
	{ opname[NMT_ROL],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 40-4f
	{ opname[NMT_RTI],ACT_NL | ADT_NONE },
	{ opname[NMT_EOR],ACT_RD | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_EOR],ACT_RD | ADT_ZEROP },
	{ opname[NMT_LSR],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_PHA],ACT_WT | ADT_NONE },
	{ opname[NMT_EOR],ACT_NL | ADT_IMM },
	{ opname[NMT_LSR],ACT_NL | ADT_AREG },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_JMP],ACT_NL | ADT_ABS },
	{ opname[NMT_EOR],ACT_RD | ADT_ABS },
	{ opname[NMT_LSR],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 50-5f
	{ opname[NMT_BVC],ACT_NL | ADT_REL },
	{ opname[NMT_EOR],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_EOR],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_LSR],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CLI],ACT_NL | ADT_NONE },
	{ opname[NMT_EOR],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_EOR],ACT_RD | ADT_ABSX },
	{ opname[NMT_LSR],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 60-6f
	{ opname[NMT_RTS],ACT_NL | ADT_NONE },
	{ opname[NMT_ADC],ACT_RD | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ADC],ACT_RD | ADT_ZEROP },
	{ opname[NMT_ROR],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_PLA],ACT_RD | ADT_NONE },
	{ opname[NMT_ADC],ACT_NL | ADT_IMM },
	{ opname[NMT_ROR],ACT_NL | ADT_AREG },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_JMP],ACT_NL | ADT_ABSIND },
	{ opname[NMT_ADC],ACT_RD | ADT_ABS },
	{ opname[NMT_ROR],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 70-7f
	{ opname[NMT_BVS],ACT_NL | ADT_REL },
	{ opname[NMT_ADC],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ADC],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_ROR],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_SEI],ACT_NL | ADT_NONE },
	{ opname[NMT_ADC],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_ADC],ACT_RD | ADT_ABSX },
	{ opname[NMT_ROR],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 80-8f
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_STA],ACT_WT | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_STY],ACT_WT | ADT_ZEROP },
	{ opname[NMT_STA],ACT_WT | ADT_ZEROP },
	{ opname[NMT_STX],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_DEY],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_TXA],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_STY],ACT_WT | ADT_ABS },
	{ opname[NMT_STA],ACT_WT | ADT_ABS },
	{ opname[NMT_STX],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// 90-9f
	{ opname[NMT_BCC],ACT_NL | ADT_REL },
	{ opname[NMT_STA],ACT_WT | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_STY],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_STA],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_STX],ACT_WT | ADT_ZEROPY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_TYA],ACT_NL | ADT_NONE },
	{ opname[NMT_STA],ACT_WT | ADT_ABSY },
	{ opname[NMT_TXS],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_STA],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// a0-af
	{ opname[NMT_LDY],ACT_NL | ADT_IMM },
	{ opname[NMT_LDA],ACT_RD | ADT_PREIND },
	{ opname[NMT_LDX],ACT_NL | ADT_IMM },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_LDY],ACT_RD | ADT_ZEROP },
	{ opname[NMT_LDA],ACT_RD | ADT_ZEROP },
	{ opname[NMT_LDX],ACT_RD | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_TAY],ACT_NL | ADT_NONE },
	{ opname[NMT_LDA],ACT_NL | ADT_IMM },
	{ opname[NMT_TAX],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_LDY],ACT_RD | ADT_ABS },
	{ opname[NMT_LDA],ACT_RD | ADT_ABS },
	{ opname[NMT_LDX],ACT_RD | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// b0-bf
	{ opname[NMT_BCS],ACT_NL | ADT_REL },
	{ opname[NMT_LDA],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_LDY],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_LDA],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_LDX],ACT_RD | ADT_ZEROPY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CLV],ACT_NL | ADT_NONE },
	{ opname[NMT_LDA],ACT_RD | ADT_ABSY },
	{ opname[NMT_TSX],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_LDY],ACT_RD | ADT_ABSX },
	{ opname[NMT_LDA],ACT_RD | ADT_ABSX },
	{ opname[NMT_LDX],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// c0-cf
	{ opname[NMT_CPY],ACT_NL | ADT_IMM },
	{ opname[NMT_CMP],ACT_RD | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CPY],ACT_RD | ADT_ZEROP },
	{ opname[NMT_CMP],ACT_RD | ADT_ZEROP },
	{ opname[NMT_DEC],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_INY],ACT_NL | ADT_NONE },
	{ opname[NMT_CMP],ACT_NL | ADT_IMM },
	{ opname[NMT_DEX],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CPY],ACT_RD | ADT_ABS },
	{ opname[NMT_CMP],ACT_RD | ADT_ABS },
	{ opname[NMT_DEC],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// d0-df
	{ opname[NMT_BNE],ACT_NL | ADT_REL },
	{ opname[NMT_CMP],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CMP],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_DEC],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CLD],ACT_NL | ADT_NONE },
	{ opname[NMT_CMP],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CMP],ACT_RD | ADT_ABSX },
	{ opname[NMT_DEC],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// e0-ef
	{ opname[NMT_CPX],ACT_NL | ADT_IMM },
	{ opname[NMT_SBC],ACT_RD | ADT_PREIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CPX],ACT_RD | ADT_ZEROP },
	{ opname[NMT_SBC],ACT_RD | ADT_ZEROP },
	{ opname[NMT_INC],ACT_WT | ADT_ZEROP },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_INX],ACT_NL | ADT_NONE },
	{ opname[NMT_SBC],ACT_NL | ADT_IMM },
	{ opname[NMT_NOP],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_CPX],ACT_RD | ADT_ABS },
	{ opname[NMT_SBC],ACT_RD | ADT_ABS },
	{ opname[NMT_INC],ACT_WT | ADT_ABS },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	// f0-ff
	{ opname[NMT_BEQ],ACT_NL | ADT_REL },
	{ opname[NMT_SBC],ACT_RD | ADT_POSTIND },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_SBC],ACT_RD | ADT_ZEROPX },
	{ opname[NMT_INC],ACT_WT | ADT_ZEROPX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_SED],ACT_NL | ADT_NONE },
	{ opname[NMT_SBC],ACT_RD | ADT_ABSY },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_UND],ACT_NL | ADT_NONE },
	{ opname[NMT_SBC],ACT_RD | ADT_ABSX },
	{ opname[NMT_INC],ACT_WT | ADT_ABSX },
	{ opname[NMT_UND],ACT_NL | ADT_NONE }
};

MemoryComment memoryCommentsTable[] = {
	{ 0x2000, 0x2000, "PPU CtrlReg.#1" },
	{ 0x2001, 0x2001, "PPU CtrlReg.#2" },
	{ 0x2002, 0x2002, "PPU Status Reg." },
	{ 0x2003, 0x2003, "SPR-RAM Addr.Reg." },
	{ 0x2004, 0x2004, "SPR-RAM I/O Reg." },
	{ 0x2005, 0x2005, "BG Scrool Reg." },
	{ 0x2006, 0x2006, "VRAM Addr.Reg." },
	{ 0x2007, 0x2007, "VRAM I/O Reg." },
	{ 0x4000, 0x4000, "SqrWave ch1 CtrlReg.#1" },
	{ 0x4001, 0x4001, "SqrWave ch1 CtrlReg.#2" },
	{ 0x4002, 0x4002, "SqrWave ch1 Freq. Reg.L" },
	{ 0x4003, 0x4003, "SqrWave ch1 Freq. Reg.H" },
	{ 0x4004, 0x4004, "SqrWave ch2 CtrlReg.#1" },
	{ 0x4005, 0x4005, "SqrWave ch2 CtrlReg.#2" },
	{ 0x4006, 0x4006, "SqrWave ch2 Freq. Reg.L" },
	{ 0x4007, 0x4007, "SqrWave ch2 Freq. Reg.H" },
	{ 0x4008, 0x4008, "TriWave ch3 CtrlReg.#1" },
	{ 0x4009, 0x4009, "TriWave ch3 CtrlReg.#2" },
	{ 0x400a, 0x400a, "TriWave ch3 Freq.Reg.L" },
	{ 0x400b, 0x400b, "TriWave ch3 Freq.Reg.H" },
	{ 0x400c, 0x400c, "NoizeWave ch4 CtrlReg.#1" },
	{ 0x400d, 0x400d, "NoizeWave ch4 CtrlReg.#2" },
	{ 0x400e, 0x400e, "NoizeWave ch4 Freq.Reg.L" },
	{ 0x400f, 0x400f, "NoizeWave ch4 Freq.Reg.H" },
	{ 0x4010, 0x4010, "DPCM CtrlReg." },
	{ 0x4011, 0x4011, "DPCM Vol.Reg." },
	{ 0x4012, 0x4012, "DPCM DMA-Addr.Reg." },
	{ 0x4013, 0x4013, "DPCM DMA-Length Reg." },
	{ 0x4014, 0x4014, "SPR-RAM DMA Reg." },
	{ 0x4015, 0x4015, "SoundChannel CtrlReg." },
	{ 0x4016, 0x4016, "Joy-Pad #1 CtrlReg." },
	{ 0x4017, 0x4017, "Joy-Pad #2 CtrlReg." },
	{ 0x408b, 0x47ff, "Unknown Register" },
	{ 0x6000, 0x7fff, "Save/Work RAM" },
	{ 0xffff, 0x0000, 0 },
};

static const char *comments[0x10000];
static bool commentSwitch[0x100];
static NesInstruction instructions[0x10000];
static char disassembled[0x10000][64];
static u8 *nesMemory = 0;

static void fillComments()
{
	memset(comments, 0, sizeof(comments));
	MemoryComment *element = memoryCommentsTable;
	for (; element->comment != 0; element++) {
		for (int i = element->begin; i <= element->end; i++)
			comments[i] = element->comment;
	}
}

static void fillCommentSwitch()
{
	for (int i = 0; i < 0x100; i++) {
		int type = dis6502[i].type;
		int adt = type & ADT_MASK;
		int act = type & ACT_MASK;
		bool on = ((adt == ADT_ABS || adt == ADT_ABSX || adt == ADT_ABSY) &&
				   (act != ACT_NL));
		commentSwitch[i] = on;
	}
}

static u8 readMem8(u16 addr)
{
	return nesMemory[addr];
}

static u16 readMem16(u16 addr)
{
	u8 lo = readMem8(addr  );
	u8 hi = readMem8(addr+1);
	return lo | (hi<<8);
}

int disassemblyOne(u16 addr)
{
	u8 op = readMem8(addr);
	int adt = dis6502[op].type & ADT_MASK;

	char *dst = disassembled[addr];

	instructions[addr].op = op;
	instructions[addr].disassembled = dst;

	int pos = 0;
	int size;
	if (addr < 0xfffa) {
		size = opByteLength[adt];
		switch (size) {
		case 1:
			if (dis6502[op].nimonic == opname[NMT_UND])
				adt = ADT_IMM;
			pos += sprintf(dst + pos, "%02x      ", op);
			pos += sprintf(dst + pos, "    %s ", dis6502[op].nimonic);
			pos += sprintf(dst + pos, adtString[adt], op);
			break;
		case 2:
			pos += sprintf(dst + pos, "%02x %02x   ", op, readMem8(addr+1));
			pos += sprintf(dst + pos, "    %s ", dis6502[op].nimonic);
			if (adt == ADT_REL) {
				u8 disp = readMem8(addr+1);
				if (disp < 0x80) {
					pos += sprintf(dst + pos, adtString[adt], addr+2+disp);
				} else {
					pos += sprintf(dst + pos, adtString[adt], addr+2-(0x100-disp));
				}
			} else {
				pos += sprintf(dst + pos, adtString[adt], readMem8(addr+1));
			}
			break;
		case 3:
			pos += sprintf(dst + pos, "%02x %02x %02x", op, readMem8(addr+1), readMem8(addr+2));
			pos += sprintf(dst + pos, "    %s ", dis6502[op].nimonic);
			pos += sprintf(dst + pos, adtString[adt], readMem16(addr+1));
			if (commentSwitch[op])
				instructions[addr].comment = comments[readMem16(addr+1)];
			break;
		default:
			UNREACHABLE();
			break;
		}
		pos += sprintf(dst + pos,"\n");
	} else {
		pos += sprintf(dst + pos," %02x      db #$%02x     \n", op, op);
		size = 1;
	}
	Q_ASSERT(pos < 64);
	return size;
}

static void disassemble(u16 begin, u16 end)
{
	for (uint i = begin; i <= end; )
		i += disassemblyOne(i);
}

NesDisassembler::NesDisassembler(u8 *nesMemory)
{
	::nesMemory = nesMemory;
	clear();
	fillComments();
	fillCommentSwitch();
}

void NesDisassembler::clear()
{
	memset(instructions, 0, sizeof(instructions));
}

QList<u16> NesDisassembler::positions() const
{
	QList<u16> result;
	result.reserve(0x8000);
	for (u16 i = 0x8000; i != 0; i++) {
		if (instructions[i].disassembled)
			result.append(i);
	}
	return result;
}

u16 NesDisassembler::nextInstruction(u16 pc) const
{
	if (pc >= 0xfffa)
		return pc + 1;
	else
		return pc + opByteLength[dis6502[readMem8(pc)].type & ADT_MASK];
}

NesInstruction NesDisassembler::instruction(u16 pc) const
{
	Q_ASSERT(0x8000 <= pc /*&& pc <= 0xffff*/);
	if (!instructions[pc].disassembled) {
		disassemble(pc, 0xffff);
		if (!instructions[0x8000].disassembled)
			disassemble(0x8000, pc);
	}
	return instructions[pc];
}
