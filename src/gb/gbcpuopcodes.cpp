#include "gbcpu.h"

#define A af.b.hi
#define F af.b.lo
#define B bc.b.hi
#define C bc.b.lo
#define D de.b.hi
#define E de.b.lo
#define H hl.b.hi
#define L hl.b.lo

#define PC pc.w
#define SP sp.w
#define AF af.w
#define BC bc.w
#define DE de.w
#define HL hl.w

#define ZFLAG(n) ( (n) ? 0 : FZ )

#define FETCH (READ(PC++))
#define FETCHW (READ(PC++) | (READ(PC++) << 8))

#define PUSH(w) ( (SP -= 2), (WRITEW(SP, (w))) )
#define POP(w) ( ((w) = READW(SP)), (SP += 2) )

#define INC(r) { ((r)++); \
F = (F & (FL|FC)) | IncFlagTable[(r)]; }

#define DEC(r) { ((r)--); \
F = (F & (FL|FC)) | DecFlagTable[(r)]; }

#define INCW(r) ( (r)++ )
#define DECW(r) ( (r)-- )

#define ADD(n) { \
TV = quint16(A) + quint16(n); \
F = (ZFLAG(V)) \
| (FH & ((A ^ (n) ^ V) << 1)) \
| (T << 4); \
A = V; }

#define ADC(n) { \
TV = quint16(A) + quint16(n) + quint16((F&FC)>>4); \
F = (ZFLAG(V)) \
| (FH & ((A ^ (n) ^ V) << 1)) \
| (T << 4); \
A = V; }

#define ADDW(n) { \
tv.d = quint32(HL) + quint32(n); \
F = (F & (FZ)) \
| (FH & ((H ^ ((n)>>8) ^ T) << 1)) \
| ((tv.d >> 16) << 4); \
HL = TV; }

#define ADDSP(n) { \
tv.d = quint32(SP) + quint32(qint8(n); \
F = (FH & (((SP>>8) ^ ((n)>>8) ^ T) << 1)) \
| ((tv.d >> 16) << 4); \
SP = TV; }

#define LDHLSP(n) { \
tv.d = quint32(SP) + quint32(qint8(n)); \
F = (FH & (((SP>>8) ^ ((n)>>8) ^ T) << 1)) \
| ((tv.d >> 16) << 4); \
HL = TV; }

#define CP(n) { \
TV = quint16(A) - quint16(n); \
F = FN \
| (ZFLAG(V)) \
| (FH & ((A ^ (n) ^ V) << 1)) \
| (quint8(-qint8(T)) << 4); }

#define SUB(n) { CP((n)); A = V; }

#define SBC(n) { \
TV = quint16(A) - quint16(n) - quint16((F&FC)>>4); \
F = FN \
| (ZFLAG((n8)V)) \
| (FH & ((A ^ (n) ^ V) << 1)) \
| (quint8(-qint8(T)) << 4); \
A = V; }

#define AND(n) { A &= (n); \
F = ZFLAG(A) | FH; }

#define XOR(n) { A ^= (n); \
F = ZFLAG(A); }

#define OR(n) { A |= (n); \
F = ZFLAG(A); }

#define RLCA(r) { (r) = ((r)>>7) | ((r)<<1); \
F = (((r)&0x01)<<4); }

#define RRCA(r) { (r) = ((r)<<7) | ((r)>>1); \
F = (((r)&0x80)>>3); }

#define RLA(r) { \
V = (((r)&0x80)>>3); \
(r) = ((r)<<1) | ((F&FC)>>4); \
F = V; }

#define RRA(r) { \
V = (((r)&0x01)<<4); \
(r) = ((r)>>1) | ((F&FC)<<3); \
F = V; }

#define RLC(r) { RLCA(r); F |= ZFLAG(r); }
#define RRC(r) { RRCA(r); F |= ZFLAG(r); }
#define RL(r) { RLA(r); F |= ZFLAG(r); }
#define RR(r) { RRA(r); F |= ZFLAG(r); }

#define SLA(r) { \
V = (((r)&0x80)>>3); \
(r) <<= 1; \
F = ZFLAG((r)) | V; }

#define SRA(r) { \
V = (((r)&0x01)<<4); \
(r) = (un8)(((n8)(r))>>1); \
F = ZFLAG((r)) | V; }

#define SRL(r) { \
V = (((r)&0x01)<<4); \
(r) >>= 1; \
F = ZFLAG((r)) | V; }

#define SWAP(r) { \
(r) = SwapTable[(r)]; \
F = ZFLAG((r)); }

#define BIT(n,r) { F = (F & FC) | ZFLAG(((r) & (1 << (n)))) | FH; }
#define RES(n,r) { (r) &= ~(1 << (n)); }
#define SET(n,r) { (r) |= (1 << (n)); }

#define JR(cond) \
if (cond) { \
	PC += 1+READ(PC); \
} else { \
	ADDCYC(-1); \
	PC++; \
}

#define JP(cond) \
if (cond) { \
	PC = READW(PC); \
} else { \
	ADDCYC(-1); \
	PC += 2; \
}

#define CALL(cond) \
if (cond) { \
	PUSH(PC+2); \
	PC = READW(PC); \
} else { \
	ADDCYC(-3); \
	PC += 2; \
}

#define RET(cond) \
if (cond) \
	POP(PC); \
else \
	ADDCYC(-3)

#define RST(n) { PUSH(PC); PC = (n); }

#define LD_GEN(base,r) \
case (base)+0: r = B; break; \
case (base)+1: r = C; break; \
case (base)+2: r = D; break; \
case (base)+3: r = E; break; \
case (base)+4: r = H; break; \
case (base)+5: r = L; break; \
case (base)+6: r = READ(HL); break; \
case (base)+7: r = A; break

#define ALU_GEN(base, imm, op) \
case (imm): V = FETCH; op(V); break; \
case (base)+0: V = B; op(V); break; \
case (base)+1: V = C; op(V); break; \
case (base)+2: V = D; op(V); break; \
case (base)+3: V = E; op(V); break; \
case (base)+4: V = H; op(V); break; \
case (base)+5: V = L; op(V); break; \
case (base)+6: V = READ(HL); op(V); break; \
case (base)+7: V = A; op(V); break;

#define CB_GEN(r, n) \
case 0x00|(n): RLC(r); break; \
case 0x08|(n): RRC(r); break; \
case 0x10|(n): RL(r); break; \
case 0x18|(n): RR(r); break; \
case 0x20|(n): SLA(r); break; \
case 0x28|(n): SRA(r); break; \
case 0x30|(n): SWAP(r); break; \
case 0x38|(n): SRL(r); break; \
case 0x40|(n): BIT(0, r); break; \
case 0x48|(n): BIT(1, r); break; \
case 0x50|(n): BIT(2, r); break; \
case 0x58|(n): BIT(3, r); break; \
case 0x60|(n): BIT(4, r); break; \
case 0x68|(n): BIT(5, r); break; \
case 0x70|(n): BIT(6, r); break; \
case 0x78|(n): BIT(7, r); break; \
case 0x80|(n): RES(0, r); break; \
case 0x88|(n): RES(1, r); break; \
case 0x90|(n): RES(2, r); break; \
case 0x98|(n): RES(3, r); break; \
case 0xA0|(n): RES(4, r); break; \
case 0xA8|(n): RES(5, r); break; \
case 0xB0|(n): RES(6, r); break; \
case 0xB8|(n): RES(7, r); break; \
case 0xC0|(n): SET(0, r); break; \
case 0xC8|(n): SET(1, r); break; \
case 0xD0|(n): SET(2, r); break; \
case 0xD8|(n): SET(3, r); break; \
case 0xE0|(n): SET(4, r); break; \
case 0xE8|(n): SET(5, r); break; \
case 0xF0|(n): SET(6, r); break; \
case 0xF8|(n): SET(7, r); break;

#define HALT m_halt = 1

#define STOP \
PC++; \
if (*KEY1 & 1) { \
	m_speed ^= 1; \
	*KEY1 = (*KEY1 & 0x7E) | (m_speed << 7); \
}
/* NOTE - we do not implement dmg STOP whatsoever */ \

#define TV tv.w
#define T tv.hi
#define V tv.lo

#define PRE_INT ( DI, PUSH(PC) )
#define THROW_INT(n) ( (*IF &= ~(1<<(n))), (PC = 0x40+((n)<<3)) )

int GBCpu::executeOne() {
	if (IME && (*IF & *IE)) {
		PRE_INT;
		switch (*IF & *IE) {
		case 0x01: case 0x03: case 0x05: case 0x07:
		case 0x09: case 0x0B: case 0x0D: case 0x0F:
		case 0x11: case 0x13: case 0x15: case 0x17:
		case 0x19: case 0x1B: case 0x1D: case 0x1F:
			THROW_INT(0); break;
		case 0x02: case 0x06: case 0x0A: case 0x0E:
		case 0x12: case 0x16: case 0x1A: case 0x1E:
			THROW_INT(1); break;
		case 0x04: case 0x0C: case 0x14: case 0x1C:
			THROW_INT(2); break;
		case 0x08: case 0x18:
			THROW_INT(3); break;
		case 0x10:
			THROW_INT(4); break;
		default:
			Q_ASSERT(false); break;
		}
	}
	IME = IMA;
	GBCpuRegister tv; // temporary register
	quint8 opcode = FETCH;
	m_instrCycles = CyclesTable[opcode];
	switch (opcode) {
	case 0x00: break;
	case 0x01: BC = FETCHW; break;
	case 0x02: WRITE(BC,A); break;
	case 0x03: INCW(BC); break;
	case 0x04: INC(B); break;
	case 0x05: DEC(B); break;
	case 0x06: B = FETCH; break;
	case 0x07: RLCA(A); break;
	case 0x08: TV = FETCHW; WRITEW(TV,SP); break;
	case 0x09: ADDW(BC); break;
	case 0x0a: A = READ(BC); break;
	case 0x0b: DECW(BC); break;
	case 0x0c: INC(C); break;
	case 0x0d: DEC(C); break;
	case 0x0e: C = FETCH; break;
	case 0x0f: RRCA(A); break;
	case 0x10: STOP; break;
	case 0x11: DE = FETCHW; break;
	case 0x12: WRITE(DE,A); break;
	case 0x13: INCW(DE); break;
	case 0x14: INC(D); break;
	case 0x15: DEC(D); break;
	case 0x16: D = FETCH; break;
	case 0x17: RLA(A); break;
	case 0x18: JR(1); break;
	case 0x19: ADDW(DE); break;
	case 0x1a: A = READ(DE); break;
	case 0x1b: DECW(DE); break;
	case 0x1c: INC(E); break;
	case 0x1d: DEC(E); break;
	case 0x1e: E = FETCH; break;
	case 0x1f: RRA(A); break;
	case 0x20: JR(!(F&FZ)); break;
	case 0x21: HL = FETCHW; break;
	case 0x22: WRITE(HL++,A); break;
	case 0x23: INCW(HL); break;
	case 0x24: INC(H); break;
	case 0x25: DEC(H); break;
	case 0x26: H = FETCH; break;
	case 0x27: /* DAA */ TV = A; TV |= (F & (FC|FH|FN)) << 4; AF = DAATable[TV]; break;
	case 0x28: JR(F&FZ); break;
	case 0x29: ADDW(HL); break;
	case 0x2a: A = READ(HL++); break;
	case 0x2b: DECW(HL); break;
	case 0x2c: INC(L); break;
	case 0x2d: DEC(L); break;
	case 0x2e: L = FETCH; break;
	case 0x2f: /* CPL */ A = ~A; F |= FN|FH; break;
	case 0x30: JR(!(F&FC)); break;
	case 0x31: SP = FETCHW; break;
	case 0x32: WRITE(HL--,A); break;
	case 0x33: INCW(SP); break;
	case 0x34: /* INC (HL) */ V=READ(HL); INC(V); WRITE(HL,V); break;
	case 0x35: /* DEC (HL) */ V=READ(HL); DEC(V); WRITE(HL,V); break;
	case 0x36: V = FETCH; WRITE(HL,V); break;
	case 0x37: /* SCF */ F = (F & FZ) | FC; break;
	case 0x38: JR(F&FC); break;
	case 0x39: ADDW(SP); break;
	case 0x3a: A = READ(HL--); break;
	case 0x3b: DECW(SP); break;
	case 0x3c: INC(A); break;
	case 0x3d: DEC(A); break;
	case 0x3e: A = FETCH; break;
	case 0x3f: /* CCF */ F = (F ^ FC) & ~(FN|FH); break;
	LD_GEN(0x40,B);
	LD_GEN(0x48,C);
	LD_GEN(0x50,D);
	LD_GEN(0x58,E);
	LD_GEN(0x60,H);
	LD_GEN(0x68,L);
	case 0x70: WRITE(HL,B); break;
	case 0x71: WRITE(HL,C); break;
	case 0x72: WRITE(HL,D); break;
	case 0x73: WRITE(HL,E); break;
	case 0x74: WRITE(HL,H); break;
	case 0x75: WRITE(HL,L); break;
	case 0x76: HALT; break;
	case 0x77: WRITE(HL,A); break;
	LD_GEN(0x78,A);
	ALU_GEN(0x80, 0xC6, ADD);
	ALU_GEN(0x88, 0xCE, ADC);
	ALU_GEN(0x90, 0xD6, SUB);
	ALU_GEN(0x98, 0xDE, SBC);
	ALU_GEN(0xA0, 0xE6, AND);
	ALU_GEN(0xA8, 0xEE, XOR);
	ALU_GEN(0xB0, 0xF6, OR);
	ALU_GEN(0xB8, 0xFE, CP);
	case 0xc0: RET(!(F&FZ)); break;
	case 0xc1: POP(BC); break;
	case 0xc2: JP(!(F&FZ)); break;
	case 0xc3: JP(1); break;
	case 0xc4: CALL(!(F&FZ)); break;
	case 0xc5: PUSH(BC); break;
	case 0xc7: RST(0x00); break;
	case 0xc8: RET(F&FZ); break;
	case 0xc9: RET(1); break;
	case 0xca: JP(F&FZ); break;
	case 0xcb:
		V = FETCH;
		m_instrCycles = CyclesTableCB[V];
		switch (V) {
		CB_GEN(B, 0);
		CB_GEN(C, 1);
		CB_GEN(D, 2);
		CB_GEN(E, 3);
		CB_GEN(H, 4);
		CB_GEN(L, 5);
		CB_GEN(A, 7);
		default:
			T = READ(HL);
			switch(cbop) {
			CB_GEN(b, 6);
			}
			if ((V & 0xC0) != 0x40) /* exclude BIT */
				WRITE(HL, T);
			break;
		}
		break;
	case 0xcc: CALL(F&FZ); break;
	case 0xcd: CALL(1); break;
	case 0xcf: RST(0x08); break;
	case 0xd0: RET(!(F&FC)); break;
	case 0xd1: POP(DE); break;
	case 0xd2: JP(!(F&FC)); break;
	//case 0xd3: // D3 illegal
	case 0xd4: CALL(!(F&FC)); break;
	case 0xd5: PUSH(DE); break;
	case 0xd7: RST(0x10); break;
	case 0xd8: RET(F&FC); break;
	case 0xd9: IME=IMA=1; RET(1); break;
	case 0xda: JP(F&FC); break;
	//case 0xdb: // DB illegal
	case 0xdc: CALL(F&FC); break;
	//case 0xdd: // DD illegal
	case 0xdf: RST(0x18); break;
	case 0xe0: V = FETCH; WRITE(0xff00+V,A); break;
	case 0xe1: POP(HL); break;
	case 0xe2: WRITE(0xff00+C,A1); break;
	//case 0xe3: // E3 illegal
	//case 0xe4: // E4 illegal
	case 0xe5: PUSH(HL); break;
	case 0xe7: RST(0x20); break;
	case 0xe8: V = FETCH; ADDSP(V); break;
	case 0xe9: PC = HL; break;
	case 0xea: TV = FETCHW; WRITE(TV,A); break;
	//case 0xeb: // EB illegal
	//case 0xec: // EC illegal
	//case 0xed: // ED illegal
	case 0xef: RST(0x28); break;
	case 0xf0: V = FETCH; A = READ(0xff00+V); break;
	case 0xf1: POP(AF); break;
	case 0xf2: A = READ(0xff00+C); break;
	case 0xf3: DI; break;
	//case 0xf4: // F4 illegal
	case 0xf5: PUSH(AF); break;
	case 0xf7: RST(0x30); break;
	case 0xf8: V = FETCH; LDHLSP(V); break;
	case 0xf9: SP = HL; break;
	case 0xfa: TV = FETCHW; A = READ(TV); break;
	case 0xfb: EI; break;
	//case 0xfc: // FC illegal
	//case 0xfd: // FD illegal
	case 0xff: RST(0x38); break;
	default:
		qDebug("invalid opcode 0x%02X at address 0x%04X",
			   opcode, (PC-1) & 0xffff);
		break;
	}
}
