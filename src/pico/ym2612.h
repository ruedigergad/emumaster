/* header file for software emulation for FM sound generator */
#ifndef PICOYM6212_H
#define PICOYM6212_H

#include <base/emu.h>

class FM_SLOT {
public:
	s32	*DT;		/* #0x00 detune          :dt_tab[DT] */
	u8	ar;			/* #0x04 attack rate  */
	u8	d1r;		/* #0x05 decay rate   */
	u8	d2r;		/* #0x06 sustain rate */
	u8	rr;			/* #0x07 release rate */
	u32	mul;		/* #0x08 multiple        :ML_TABLE[ML] */

	/* Phase Generator */
	u32	phase;		/* #0x0c phase counter */
	u32	Incr;		/* #0x10 phase step */

	u8	KSR;		/* #0x14 key scale rate  :3-KSR */
	u8	ksr;		/* #0x15 key scale rate  :kcode>>(3-KSR) */

	u8	key;		/* #0x16 0=last key was KEY OFF, 1=KEY ON */

	/* Envelope Generator */
	u8	state;		/* #0x17 phase type: EG_OFF=0, EG_REL, EG_SUS, EG_DEC, EG_ATT */
	u16	tl;			/* #0x18 total level: TL << 3 */
	s16	volume;		/* #0x1a envelope counter */
	u32	sl;			/* #0x1c sustain level:sl_table[SL] */

	u32	eg_pack_ar;		/* #0x20 (attack state) */
	u32	eg_pack_d1r;	/* #0x24 (decay state) */
	u32	eg_pack_d2r;	/* #0x28 (sustain state) */
	u32	eg_pack_rr;		/* #0x2c (release state) */
};


class FM_CH {
public:
	FM_SLOT	fmSlot[4];	/* four SLOTs (operators) */

	u8	ALGO;		/* algorithm */
	u8	FB;			/* feedback shift */
	s32	op1_out;	/* op1 output for feedback */

	s32	mem_value;	/* delayed sample (MEM) value */

	s32	pms;		/* channel PMS */
	u8	ams;		/* channel AMS */

	u8	kcode;		/* key code:                        */
	u32	fc;			/* fnum,blk:adjusted to sample rate */
	u32	block_fnum;	/* current blk/fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */

	/* LFO */
	u8	AMmasks;	/* AM enable flag */

};

class FM_ST {
public:
	int		clock;		/* master clock  (Hz)   */
	int		rate;		/* sampling rate (Hz)   */
	qreal	freqbase;	/* 08 frequency base       */
	u8	address;	/* 10 address register     */
	u8	status;		/* 11 status flag          */
	u8	mode;		/* mode  CSM / 3SLOT    */
	u8	fn_h;		/* freq latch           */
	int		TA;			/* timer a              */
	int		TAC;		/* timer a maxval       */
	int		TAT;		/* timer a ticker       */
	u8	TB;			/* timer b              */
	u8	pad[3];
	int		TBC;		/* timer b maxval       */
	int		TBT;		/* timer b ticker       */
	/* local time tables */
	s32	dt_tab[8][32];/* DeTune table       */
};

/***********************************************************/
/* OPN unit                                                */
/***********************************************************/

/* OPN 3slot struct */
class FM_3SLOT {
public:
	u32  fc[3];			/* fnum3,blk3: calculated */
	u8	fn_h;			/* freq3 latch */
	u8	kcode[3];		/* key code */
	u32	block_fnum[3];	/* current fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */
};

/* OPN/A/B common state */
class FM_OPN {
public:
	FM_ST	ST;				/* general state */
	FM_3SLOT SL3;			/* 3 slot mode state */
	u32  pan;			/* fm channels output mask (bit 1 = enable) */

	u32	eg_cnt;			/* #0xb38 global envelope generator counter */
	u32	eg_timer;		/* #0xb3c global envelope generator counter works at frequency = chipclock/64/3 */
	u32	eg_timer_add;	/* #0xb40 step of eg_timer */

	/* LFO */
	u32	lfo_cnt;
	u32	lfo_inc;

	u32	lfo_freq[8];	/* LFO FREQ table */
};

/* here's the virtual YM2612 */
class YM2612 {
public:
	void sl(const QString &name);

	u8		REGS[0x200];		/* registers (for save states)       */
	s32		addr_A1;			/* address line A1      */

	FM_CH		CH[6];				/* channel state (0x168 bytes each)? */

	/* dac output (YM2612) */
	int			dacen;
	s32		dacout;

	FM_OPN		OPN;				/* OPN state            */

	u32		slot_mask;			/* active slot mask (performance hack) */
};

extern int   *ym2612_dacen;
extern s32 *ym2612_dacout;
extern FM_ST *ym2612_st;

static inline u8 YM2612Read() {
	return ym2612_st->status;
}

static inline void YM2612PicoTick(int n) {
	// timer A
	if ((ym2612_st->mode & 0x01) &&
		(ym2612_st->TAT+=64*n) >= ym2612_st->TAC) {
		ym2612_st->TAT -= ym2612_st->TAC;
		if (ym2612_st->mode & 0x04)
			ym2612_st->status |= 1;
	}
	// timer B
	if ((ym2612_st->mode & 0x02) &&
		(ym2612_st->TBT+=64*n) >= ym2612_st->TBC) {
		ym2612_st->TBT -= ym2612_st->TBC;
		if (ym2612_st->mode & 0x08)
			ym2612_st->status |= 2;
	}
}

extern YM2612 ym2612;

void YM2612Init(int baseclock, int rate);
void YM2612ResetChip();
void YM2612Update(int *buffer, int length);
void YM2612PicoStateLoad();
extern "C" int YM2612Write(unsigned int a, unsigned int v);

#endif // PICOYM6212_H
