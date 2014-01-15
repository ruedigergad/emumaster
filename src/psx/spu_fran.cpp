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

#include "spu_fran.h"
#include "spu_registers.h"
#include "mem.h"
#include "psx.h"
#include <QDataStream>

PsxSpuFran psxSpuFran;


// num of channels
#define MAXCHAN     24

///////////////////////////////////////////////////////////
// struct defines
///////////////////////////////////////////////////////////

// ADSR INFOS PER CHANNEL
typedef struct
{
 int            AttackModeExp;
 long           AttackTime;
 long           DecayTime;
 long           SustainLevel;
 int            SustainModeExp;
 long           SustainModeDec;
 long           SustainTime;
 int            ReleaseModeExp;
 unsigned long  ReleaseVal;
 long           ReleaseTime;
 long           ReleaseStartTime;
 long           ReleaseVol;
 long           lTime;
 long           lVolume;
} ADSRInfo;

typedef struct
{
 int            State;
 int            AttackModeExp;
 int            AttackRate;
 int            DecayRate;
 int            SustainLevel;
 int            SustainModeExp;
 int            SustainIncrease;
 int            SustainRate;
 int            ReleaseModeExp;
 int            ReleaseRate;
 int            EnvelopeVol;
 long           lVolume;
 long           lDummy1;
 long           lDummy2;
} ADSRInfoEx;

///////////////////////////////////////////////////////////

// MAIN CHANNEL STRUCT
typedef struct
{
 int               bNew;                               // start flag

 int               iSBPos;                             // mixing stuff
 int               spos;
 int               sinc;
 int               SB[32];                             // Pete added another 32 dwords in 1.6 ... prevents overflow issues with gaussian/cubic interpolation (thanx xodnizel!), and can be used for even better interpolations, eh? :)
 int               sval;

 unsigned char *   pStart;                             // start ptr into sound mem
 unsigned char *   pCurr;                              // current pos in sound mem
 unsigned char *   pLoop;                              // loop ptr in sound mem

 int               bOn;                                // is channel active (sample playing?)
 int               bStop;                              // is channel stopped (sample _can_ still be playing, ADSR Release phase)
 int               iActFreq;                           // current psx pitch
 int               iUsedFreq;                          // current pc pitch
 int               iLeftVolume;                        // left volume
 int               bIgnoreLoop;                        // ignore loop bit, if an external loop address is used
 int               iRightVolume;                       // right volume
 int               iRawPitch;                          // raw pitch (0...3fff)
 int               s_1;                                // last decoding infos
 int               s_2;
 int               bNoise;                             // noise active flag
 int               bFMod;                              // freq mod (0=off, 1=sound channel, 2=freq channel)
 int               iOldNoise;                          // old noise val for this channel
 ADSRInfo          ADSR;                               // active ADSR settings
 ADSRInfoEx        ADSRX;                              // next ADSR settings (will be moved to active on sample start)

} SPUCHAN;

///////////////////////////////////////////////////////////

typedef struct
{
 int StartAddr;      // reverb area start addr in samples
 int CurrAddr;       // reverb area curr addr in samples

 int VolLeft;
 int VolRight;
 int iLastRVBLeft;
 int iLastRVBRight;
 int iRVBLeft;
 int iRVBRight;


 int FB_SRC_A;       // (offset)
 int FB_SRC_B;       // (offset)
 int IIR_ALPHA;      // (coef.)
 int ACC_COEF_A;     // (coef.)
 int ACC_COEF_B;     // (coef.)
 int ACC_COEF_C;     // (coef.)
 int ACC_COEF_D;     // (coef.)
 int IIR_COEF;       // (coef.)
 int FB_ALPHA;       // (coef.)
 int FB_X;           // (coef.)
 int IIR_DEST_A0;    // (offset)
 int IIR_DEST_A1;    // (offset)
 int ACC_SRC_A0;     // (offset)
 int ACC_SRC_A1;     // (offset)
 int ACC_SRC_B0;     // (offset)
 int ACC_SRC_B1;     // (offset)
 int IIR_SRC_A0;     // (offset)
 int IIR_SRC_A1;     // (offset)
 int IIR_DEST_B0;    // (offset)
 int IIR_DEST_B1;    // (offset)
 int ACC_SRC_C0;     // (offset)
 int ACC_SRC_C1;     // (offset)
 int ACC_SRC_D0;     // (offset)
 int ACC_SRC_D1;     // (offset)
 int IIR_SRC_B1;     // (offset)
 int IIR_SRC_B0;     // (offset)
 int MIX_DEST_A0;    // (offset)
 int MIX_DEST_A1;    // (offset)
 int MIX_DEST_B0;    // (offset)
 int MIX_DEST_B1;    // (offset)
 int IN_COEF_L;      // (coef.)
 int IN_COEF_R;      // (coef.)
} REVERBInfo;

static u16 regArea[10000];
static u16 spuMem[256*1024];
static u8 *spuMemC;
static u8 *pSpuBuffer;
static u8 *pMixIrq=0;

// infos struct for each channel
static SPUCHAN         s_chan[MAXCHAN+1];                     // channel + 1 infos (1 is security for fmod handling)
static REVERBInfo      rvb;

static u16 spuCtrl = 0;                             // some vars to store psx reg infos
static u16 spuStat = 0;
static u16 spuIrq = 0;
static u32 spuAddr = 0xFFFFFFFF;                    // address into spu mem

// TODO
int iFMod[44100];

unsigned long * XAFeed  = NULL;
unsigned long * XAPlay  = NULL;
unsigned long * XAStart = NULL;
unsigned long * XAEnd   = NULL;
unsigned long   XARepeat  = 0;

int             iLeftXAVol  = 32767;
int             iRightXAVol = 32767;

static u16 spuFranReadDMA() {
	u16 data = spuMem[spuAddr >> 1];
	spuAddr = (spuAddr + 2) & 0x7FFFF;
	return data;
}

static void spuFranWriteDMA(u16 data) {
	spuMem[spuAddr>>1] = data;
	spuAddr = (spuAddr + 2) & 0x7FFFF;
}

static void spuFranReadDMAMem(u16 *pusPSXMem, int size) {
	if (spuAddr + (size<<1) >= 0x80000) {
		memcpy(pusPSXMem, &spuMem[spuAddr>>1], 0x7FFFF-spuAddr+1);
		memcpy(pusPSXMem+(0x7FFFF-spuAddr+1), spuMem, (size<<1)-(0x7FFFF-spuAddr+1));
		spuAddr = (size<<1) - (0x7FFFF-spuAddr+1);
	} else {
		memcpy(pusPSXMem,&spuMem[spuAddr>>1], (size<<1));
		spuAddr += (size<<1);
	}
}

static void spuFranWriteDMAMem(u16 *pusPSXMem, int size) {
	if (spuAddr+(size<<1)>0x7FFFF) {
		memcpy(&spuMem[spuAddr>>1], pusPSXMem, 0x7FFFF-spuAddr+1);
		memcpy(spuMem, pusPSXMem+(0x7FFFF-spuAddr+1), (size<<1)-(0x7FFFF-spuAddr+1));
		spuAddr = (size<<1)-(0x7FFFF-spuAddr+1);
	} else {
		memcpy(&spuMem[spuAddr>>1], pusPSXMem, (size<<1));
		spuAddr += (size<<1);
	}
}

static inline void SoundOn(int start, int end, u16 data) {
	for (int ch = start; ch < end; ch++,data>>=1) {
		if ((data&1) && s_chan[ch].pStart) {
			s_chan[ch].bIgnoreLoop=0;
			s_chan[ch].bNew=1;
		}
	}
}

static inline void SoundOff(int start,int end, u16 data) {
	for (int ch = start; ch < end; ch++,data>>=1)
		s_chan[ch].bStop |= (data & 1);
}

static inline void FModOn(int start,int end, u16 data) {
	for (int ch = start; ch < end; ch++,data>>=1) {
		if (data & 1) {
			if (ch > 0) {
				s_chan[ch].bFMod = 1;
				s_chan[ch-1].bFMod=2;
			}
		} else {
			s_chan[ch].bFMod = 0;
		}
	}
}

static inline void NoiseOn(int start,int end, u16 data) {
	for (int ch=start;ch<end;ch++,data>>=1)
		s_chan[ch].bNoise = data & 1;
}

static inline int calcVolume(s16 vol) {
	if (vol & 0x8000) {
		int sInc=1;
		if (vol & 0x2000)
			sInc = -1;
		if (vol & 0x1000)
			vol ^= 0xFFFF;
		vol = ((vol&0x7F)+1)/2;
		vol += vol/(2*sInc);
		vol *= 128;
	} else {
		if (vol & 0x4000)
			vol = 0x3FFF - (vol&0x3FFF);
	}
	vol &= 0x3FFF;
	return vol;
}

static inline void setPitch(int ch, int val) {
	val = qMin(val, 0x3FFF);
	s_chan[ch].iRawPitch = val;
	val = (44100*val) / 4096;
	if (val < 1)
		val = 1;
	s_chan[ch].iActFreq = val;
}

static void spuFranWriteRegister(u32 reg, u16 data) {
	reg &= 0xFFF;
	regArea[(reg-0xC00)>>1] = data;
	if (reg>=0x0C00 && reg<0x0D80) {
		int ch = (reg>>4) - 0xC0;
		switch (reg&0x0F) {
		case 0:
			s_chan[ch].iLeftVolume = calcVolume(data);
			break;
		case 2:
			s_chan[ch].iRightVolume = calcVolume(data);
			break;
		case 4:
			setPitch(ch, data);
			break;
		case 6:
			s_chan[ch].pStart = spuMemC + (data<<3);
			break;
		case 8:
			s_chan[ch].ADSRX.AttackModeExp=(data&0x8000) >> 15;
			s_chan[ch].ADSRX.AttackRate = ((data>>8) & 0x007F)^0x7F;
			s_chan[ch].ADSRX.DecayRate = 4*(((data>>4) & 0x000F)^0x1F);
			s_chan[ch].ADSRX.SustainLevel = (data & 0x000F) << 27;
			break;
		case 10:
			s_chan[ch].ADSRX.SustainModeExp = (data&0x8000) >> 15;
			s_chan[ch].ADSRX.SustainIncrease= (data&0x4000) >> 14;
			s_chan[ch].ADSRX.SustainRate = ((data>>6) & 0x007F)^0x7F;
			s_chan[ch].ADSRX.ReleaseModeExp = (data&0x0020) >> 5;
			s_chan[ch].ADSRX.ReleaseRate = 4*((data & 0x001F)^0x1F);
			break;
		case 12:
			break;
		case 14:
			s_chan[ch].pLoop = spuMemC + (data<<3);
			s_chan[ch].bIgnoreLoop = 1;
			break;
		}
		return;
	}
	switch (reg) {
	case H_SPUaddr:
		spuAddr = data << 3;
		break;
	case H_SPUdata:
		spuMem[spuAddr>>1] = data;
		spuAddr += 2;
		spuAddr &= 0x7FFFF;
		break;
	case H_SPUctrl    : spuCtrl=data; 		break;
	case H_SPUstat    : spuStat=data & 0xF800; 	break;
	case H_SPUReverbAddr:
		if (data==0xFFFF || data<=0x200) {
			rvb.StartAddr=rvb.CurrAddr=0;
		} else {
			if (rvb.StartAddr != (data<<2)) {
				rvb.StartAddr = data<<2;
				rvb.CurrAddr=rvb.StartAddr;
			}
		}
		break;
	case H_SPUirqAddr : spuIrq = data;		break;
	case H_SPUrvolL   : rvb.VolLeft=data; 		break;
	case H_SPUrvolR   : rvb.VolRight=data; 		break;
	case H_SPUon1     : SoundOn(0,16,data); 		break;
	case H_SPUon2     : SoundOn(16,24,data); 	break;
	case H_SPUoff1    : SoundOff(0,16,data); 	break;
	case H_SPUoff2    : SoundOff(16,24,data); 	break;
	case H_CDLeft     : iLeftXAVol=data  & 0x7FFF; 	break;
	case H_CDRight    : iRightXAVol=data & 0x7FFF; 	break;
	case H_FMod1      : FModOn(0,16,data); 		break;
	case H_FMod2      : FModOn(16,24,data); 		break;
	case H_Noise1     : NoiseOn(0,16,data); 		break;
	case H_Noise2     : NoiseOn(16,24,data); 	break;
	case H_RVBon1	  : break;
	case H_RVBon2	  : break;
	case H_Reverb+0   : rvb.FB_SRC_A=data;	  break;
	case H_Reverb+2   : rvb.FB_SRC_B=data;    break;
	case H_Reverb+4   : rvb.IIR_ALPHA=data;   break;
	case H_Reverb+6   : rvb.ACC_COEF_A=data;  break;
	case H_Reverb+8   : rvb.ACC_COEF_B=data;  break;
	case H_Reverb+10  : rvb.ACC_COEF_C=data;  break;
	case H_Reverb+12  : rvb.ACC_COEF_D=data;  break;
	case H_Reverb+14  : rvb.IIR_COEF=data;    break;
	case H_Reverb+16  : rvb.FB_ALPHA=data;    break;
	case H_Reverb+18  : rvb.FB_X=data;        break;
	case H_Reverb+20  : rvb.IIR_DEST_A0=data; break;
	case H_Reverb+22  : rvb.IIR_DEST_A1=data; break;
	case H_Reverb+24  : rvb.ACC_SRC_A0=data;  break;
	case H_Reverb+26  : rvb.ACC_SRC_A1=data;  break;
	case H_Reverb+28  : rvb.ACC_SRC_B0=data;  break;
	case H_Reverb+30  : rvb.ACC_SRC_B1=data;  break;
	case H_Reverb+32  : rvb.IIR_SRC_A0=data;  break;
	case H_Reverb+34  : rvb.IIR_SRC_A1=data;  break;
	case H_Reverb+36  : rvb.IIR_DEST_B0=data; break;
	case H_Reverb+38  : rvb.IIR_DEST_B1=data; break;
	case H_Reverb+40  : rvb.ACC_SRC_C0=data;  break;
	case H_Reverb+42  : rvb.ACC_SRC_C1=data;  break;
	case H_Reverb+44  : rvb.ACC_SRC_D0=data;  break;
	case H_Reverb+46  : rvb.ACC_SRC_D1=data;  break;
	case H_Reverb+48  : rvb.IIR_SRC_B1=data;  break;
	case H_Reverb+50  : rvb.IIR_SRC_B0=data;  break;
	case H_Reverb+52  : rvb.MIX_DEST_A0=data; break;
	case H_Reverb+54  : rvb.MIX_DEST_A1=data; break;
	case H_Reverb+56  : rvb.MIX_DEST_B0=data; break;
	case H_Reverb+58  : rvb.MIX_DEST_B1=data; break;
	case H_Reverb+60  : rvb.IN_COEF_L=data;   break;
	case H_Reverb+62  : rvb.IN_COEF_R=data;   break;
	}
}

static u16 spuFranReadRegister(u32 reg) {
	reg &= 0xFFF;
	if (reg>=0x0C00 && reg<0x0D80) {
		int ch=(reg>>4)-0xc0;
		switch (reg&0x0F) {
		case 12: {
			if (s_chan[ch].bNew)
				return 1;
			if (s_chan[ch].ADSRX.lVolume && !s_chan[ch].ADSRX.EnvelopeVol)
				return 1;
			return s_chan[ch].ADSRX.EnvelopeVol >> 16;
		case 14:
			if (!s_chan[ch].pLoop)
				return 0;
			return (s_chan[ch].pLoop-spuMemC) >> 3;
		}
		}
	}

	switch(reg) {
	case H_SPUctrl: return spuCtrl;
	case H_SPUstat: return spuStat;
	case H_SPUaddr: return spuAddr>>3;
	case H_SPUdata: {
		u16 s = spuMem[spuAddr>>1];
		spuAddr += 2;
		spuAddr &= 0x7FFFF;
		return s;
	}
	case H_SPUirqAddr: return spuIrq;
	}
	return regArea[(reg-0xc00)>>1];
}

// MIX XA
static inline void MixXA(u16 *dst, int nSamples) {
	int i;
	unsigned long XALastVal;
	int leftvol =iLeftXAVol;
	int rightvol=iRightXAVol;

	for(i=0;i<nSamples && XAPlay!=XAFeed;i++)
	{
		XALastVal=*XAPlay++;
		if(XAPlay==XAEnd) XAPlay=XAStart;
		(*dst++)+=(((short)(XALastVal&0xffff))       * leftvol)>>15;
		(*dst++)+=(((short)((XALastVal>>16)&0xffff)) * rightvol)>>15;
	}

	if(XAPlay==XAFeed && XARepeat)
	{
		XARepeat--;
		for(;i<nSamples;i++)
			{
				(*dst++)+=(((short)(XALastVal&0xffff))       * leftvol)>>15;
				(*dst++)+=(((short)((XALastVal>>16)&0xffff)) * rightvol)>>15;
			}
	}
}

static unsigned long int RateTable[160];

/* INIT ADSR */
void InitADSR(void)
{
	unsigned long r=3,rs=1,rd=0;
	int i;
	memset(RateTable,0,sizeof(unsigned long)*160);        // build the rate table according to Neill's rules (see at bottom of file)

	for(i=32;i<160;i++)                                   // we start at pos 32 with the real values... everything before is 0
	{
		if(r<0x3FFFFFFF)
			{
				r+=rs;
				rd++;
				if(rd==5) {
					rd=1;
					rs*=2;
				}
			}
		if(r>0x3FFFFFFF)
			r=0x3FFFFFFF;
		RateTable[i]=r;
	}
}

static const unsigned long int TableDisp[] = {
 -0x18+0+32,-0x18+4+32,-0x18+6+32,-0x18+8+32,       // release/decay
 -0x18+9+32,-0x18+10+32,-0x18+11+32,-0x18+12+32,

 -0x1B+0+32,-0x1B+4+32,-0x1B+6+32,-0x1B+8+32,       // sustain
 -0x1B+9+32,-0x1B+10+32,-0x1B+11+32,-0x1B+12+32,
};


/* MIX ADSR */
static inline int MixADSR(SPUCHAN *ch) {
	u32 disp;
	s32 EnvelopeVol = ch->ADSRX.EnvelopeVol;

	if (ch->bStop) {
		if(ch->ADSRX.ReleaseModeExp)
				disp = TableDisp[(EnvelopeVol>>28)&0x7];
		else
				disp=-0x0C+32;

		EnvelopeVol-=RateTable[ch->ADSRX.ReleaseRate + disp];

		if(EnvelopeVol<0)
			{
				EnvelopeVol=0;
				ch->bOn=0;
			}

		ch->ADSRX.EnvelopeVol=EnvelopeVol;
		ch->ADSRX.lVolume=(EnvelopeVol>>=21);
		return EnvelopeVol;
	}
	else                                                  // not stopped yet?
	{
		if(ch->ADSRX.State==0)                       // -> attack
			{
				disp = -0x10+32;
				if(ch->ADSRX.AttackModeExp)
				{
					if(EnvelopeVol>=0x60000000)
						disp = -0x18+32;
				}
				EnvelopeVol+=RateTable[ch->ADSRX.AttackRate+disp];

				if(EnvelopeVol<0)
				{
					EnvelopeVol=0x7FFFFFFF;
					ch->ADSRX.State=1;
				}

				ch->ADSRX.EnvelopeVol=EnvelopeVol;
				ch->ADSRX.lVolume=(EnvelopeVol>>=21);
				return EnvelopeVol;
			}

		if(ch->ADSRX.State==1)                       // -> decay
			{
				disp = TableDisp[(EnvelopeVol>>28)&0x7];
				EnvelopeVol-=RateTable[ch->ADSRX.DecayRate+disp];

				if(EnvelopeVol<0)
					EnvelopeVol=0;
				if(EnvelopeVol <= ch->ADSRX.SustainLevel)
					ch->ADSRX.State=2;

				ch->ADSRX.EnvelopeVol=EnvelopeVol;
				ch->ADSRX.lVolume=(EnvelopeVol>>=21);
				return EnvelopeVol;
			}

		if(ch->ADSRX.State==2)                       // -> sustain
			{
				if(ch->ADSRX.SustainIncrease)
				{
					disp = -0x10+32;
					if(ch->ADSRX.SustainModeExp)
					{
						if(EnvelopeVol>=0x60000000)
							disp = -0x18+32;
					}
					EnvelopeVol+=RateTable[ch->ADSRX.SustainRate+disp];

					if(EnvelopeVol<0)
						EnvelopeVol=0x7FFFFFFF;
				}
				else
				{
					if(ch->ADSRX.SustainModeExp)
						disp = TableDisp[((EnvelopeVol>>28)&0x7)+8];
					else
						disp=-0x0F+32;

					EnvelopeVol-=RateTable[ch->ADSRX.SustainRate+disp];

					if(EnvelopeVol<0)
						EnvelopeVol=0;
				}

				ch->ADSRX.EnvelopeVol=EnvelopeVol;
				ch->ADSRX.lVolume=(EnvelopeVol>>=21);
				return EnvelopeVol;
			}
	}
	return 0;
}

// FEED XA
inline void FeedXA(xa_decode_t *xap)
{
	int sinc,spos,i,iSize;
	XARepeat  = 100;                                      // set up repeat

	iSize=((44100*xap->nsamples)/xap->freq);              // get size
	if(!iSize) return;                                    // none? bye

	if(XAFeed<XAPlay) {
		if ((XAPlay-XAFeed)==0) return;               // how much space in my buf?
	} else {
		if (((XAEnd-XAFeed) + (XAPlay-XAStart))==0) return;
	}

	spos=0x10000L;
	sinc = (xap->nsamples << 16) / iSize;                 // calc freq by num / size

	if(xap->stereo)
	{
		unsigned long * pS=(unsigned long *)xap->pcm;
		unsigned long l=0;

		for(i=0;i<iSize;i++)
			{
				while(spos>=0x10000L)
				{
					l = *pS++;
					spos -= 0x10000L;
				}

				*XAFeed++=l;

				if(XAFeed==XAEnd)
					XAFeed=XAStart;
				if(XAFeed==XAPlay)
				{
					if(XAPlay!=XAStart)
						XAFeed=XAPlay-1;
					break;
				}

				spos += sinc;
			}
	}
	else
	{
		unsigned short * pS=(unsigned short *)xap->pcm;
		unsigned long l;short s=0;

		for(i=0;i<iSize;i++)
			{
				while(spos>=0x10000L)
				{
					s = *pS++;
					spos -= 0x10000L;
				}
				l=s;

				*XAFeed++=(l|(l<<16));

				if(XAFeed==XAEnd)
					XAFeed=XAStart;
				if(XAFeed==XAPlay)
				{
					if(XAPlay!=XAStart)
						XAFeed=XAPlay-1;
					break;
				}

				spos += sinc;
			}
	}
}

static inline void StartSound(SPUCHAN * pChannel) {
	pChannel->ADSRX.lVolume=1;                            // Start ADSR
	pChannel->ADSRX.State=0;
	pChannel->ADSRX.EnvelopeVol=0;
	pChannel->pCurr=pChannel->pStart;                     // set sample start
	pChannel->s_1=0;                                      // init mixing vars
	pChannel->s_2=0;
	pChannel->iSBPos=28;
	pChannel->bNew=0;                                     // init channel flags
	pChannel->bStop=0;
	pChannel->bOn=1;
	pChannel->SB[29]=0;                                   // init our interpolation helpers
	pChannel->SB[30]=0;
	pChannel->spos=0x10000L;
	pChannel->SB[31]=0;    				// -> no/simple interpolation starts with one 44100 decoding
}

static inline void VoiceChangeFrequency(SPUCHAN * pChannel) {
	pChannel->iUsedFreq=pChannel->iActFreq;               // -> take it and calc steps
	pChannel->sinc=pChannel->iRawPitch<<4;
	if(!pChannel->sinc) pChannel->sinc=1;
}

inline void FModChangeFrequency(SPUCHAN * pChannel,int ns)
{
	int NP=pChannel->iRawPitch;
	NP=((32768L+iFMod[ns])*NP)/32768L;
	if(NP>0x3FFF) NP=0x3FFF;
	if(NP<0x1)    NP=0x1;
	NP=(44100L*NP)/(4096L);                               // calc frequency
	pChannel->iActFreq=NP;
	pChannel->iUsedFreq=NP;
	pChannel->sinc=(((NP/10)<<16)/4410);
	if(!pChannel->sinc) pChannel->sinc=1;
	iFMod[ns]=0;
}

inline void StoreInterpolationVal(SPUCHAN * pChannel,int fa)
{
	if(pChannel->bFMod==2)                                	// fmod freq channel
		pChannel->SB[29]=fa;
	else
	{
		if((spuCtrl&0x4000)==0)
			fa=0;                       		// muted?
		else                                            // else adjust
			{
				if(fa>32767L)  fa=32767L;
				if(fa<-32767L) fa=-32767L;
			}
		pChannel->SB[29]=fa;                           	// no interpolation
	}
}

static void spuFranPlayADPCMchannel(xa_decode_t *xap) {
	Q_ASSERT(xap != 0);
	if (Config.Xa && xap->freq)
		FeedXA(xap); // call main XA feeder
}

static void spuFranPlayCDDAchannel(s16 *, int) {
}

bool PsxSpuFran::init() {
	SPU_writeRegister		= spuFranWriteRegister;
	SPU_readRegister		= spuFranReadRegister;
	SPU_writeDMA			= spuFranWriteDMA;
	SPU_readDMA				= spuFranReadDMA;
	SPU_writeDMAMem			= spuFranWriteDMAMem;
	SPU_readDMAMem			= spuFranReadDMAMem;
	SPU_playADPCMchannel	= spuFranPlayADPCMchannel;
	SPU_playCDDAchannel		= spuFranPlayCDDAchannel;

	spuMemC=(unsigned char *)spuMem;                      // just small setup
	memset((void *)s_chan,0,MAXCHAN*sizeof(SPUCHAN));
	memset((void *)&rvb,0,sizeof(REVERBInfo));
	InitADSR();

	spuIrq=0;
	spuAddr=0xFFFFFFFF;
	spuMemC=(unsigned char *)spuMem;
	pMixIrq=0;
	memset((void *)s_chan,0,(MAXCHAN+1)*sizeof(SPUCHAN));

	//Setup streams
	pSpuBuffer=(unsigned char *)malloc(32768);            // alloc mixing buffer
	XAStart = (unsigned long *)malloc(44100*4);           // alloc xa buffer
	XAPlay  = XAStart;
	XAFeed  = XAStart;
	XAEnd   = XAStart + 44100;
	for(int i=0;i<MAXCHAN;i++)                                // loop sound channels
	{
		s_chan[i].ADSRX.SustainLevel = 0xf<<27;       // -> init sustain
		s_chan[i].pLoop=spuMemC;
		s_chan[i].pStart=spuMemC;
		s_chan[i].pCurr=spuMemC;
	}
	memset(iFMod, 0, sizeof(iFMod));
	return true;
}

void PsxSpuFran::shutdown() {
	free(pSpuBuffer);
	pSpuBuffer = 0;
	free(XAStart);
	XAStart = 0;
}

int PsxSpuFran::fillBuffer(char *stream, int size) {
	int sampleCount = psxEmu.systemType == PsxEmu::NtscType ? 44100/60 : 44100/50;
	sampleCount = qMin(size/4, sampleCount);
	memset(stream, 0, sampleCount*4);

	int s_1,s_2,fa;
	int predict_nr,shift_factor,flags,s;
	static const int f[5][2] = {{0,0},{60,0},{115,-52},{98,-55},{122,-60}};

	u16 *dst = (u16 *)stream;
	for (SPUCHAN *ch = s_chan; ch != s_chan + MAXCHAN; ch++) {
		if (ch->bNew)
			StartSound(ch);
		if (!ch->bOn)
			continue;
		if (ch->iActFreq != ch->iUsedFreq)
			VoiceChangeFrequency(ch);
		for (int ns = 0; ns < sampleCount; ns++) {
			if (ch->bFMod==1 && iFMod[ns])     // fmod freq channel
				FModChangeFrequency(ch, ns);
			while (ch->spos >= 0x10000L) {
				if (ch->iSBPos == 28) {
					if (ch->pCurr == (u8 *)-1) // special "stop" sign
					{
						ch->bOn=0; // -> turn everything off
						ch->ADSRX.lVolume=0;
						ch->ADSRX.EnvelopeVol=0;
						goto ENDX;       // -> and done for this channel
					}
					ch->iSBPos=0;
					s_1=ch->s_1;
					s_2=ch->s_2;
					predict_nr=(int)*ch->pCurr;
					ch->pCurr++;
					shift_factor=predict_nr&0xf;
					predict_nr >>= 4;
					flags=(int)*ch->pCurr;
					ch->pCurr++;

					for (int i=0;i<28;ch->pCurr++) {
						s=((((int)*ch->pCurr)&0xf)<<12);
						if(s&0x8000) s|=0xFFFF0000;
						fa=(s >> shift_factor);
						fa=fa + ((s_1 * f[predict_nr][0])>>6) + ((s_2 * f[predict_nr][1])>>6);
						s_2=s_1;s_1=fa;
						s=((((int)*ch->pCurr) & 0xf0) << 8);
						ch->SB[i++]=fa;
						if(s&0x8000) s|=0xFFFF0000;
						fa=(s>>shift_factor);
						fa=fa + ((s_1 * f[predict_nr][0])>>6) + ((s_2 * f[predict_nr][1])>>6);
						s_2=s_1;s_1=fa;
						ch->SB[i++]=fa;
					}

					// flag handler
					if ((flags&4) && (!ch->bIgnoreLoop))
						ch->pLoop=ch->pCurr-16;                // loop address
					if (flags&1)                               	// 1: stop/loop
					{
						// We play this block out first...
						if(flags!=3 || ch->pLoop==NULL)   // PETE: if we don't check exactly for 3, loop hang ups will happen (DQ4, for example)
							ch->pCurr = (u8*)-1;	// and checking if pLoop is set avoids crashes, yeah
						else
							ch->pCurr = ch->pLoop;
					}
					ch->s_1=s_1;
					ch->s_2=s_2;
				}
				fa=ch->SB[ch->iSBPos++];        // get sample data
				StoreInterpolationVal(ch,fa);         // store val for later interpolation
				ch->spos -= 0x10000L;
			}

			if (ch->bNoise)
				fa = 0;
			else
				fa = ch->SB[29];       		// get interpolation val
			ch->sval = (MixADSR(ch)*fa)>>10;   // mix adsr
			if (ch->bFMod==2) {                       // fmod freq channel
				iFMod[ns]=ch->sval;             // -> store 1T sample data, use that to do fmod on next channel
			} else {
				dst[ns*2+0]+=(ch->sval*ch->iLeftVolume)>>14;
				dst[ns*2+1]+=(ch->sval*ch->iRightVolume)>>14;
			}
			ch->spos += ch->sinc;
		}
		ENDX: ;
	}
	if (XAPlay!=XAFeed || XARepeat)
		MixXA((u16 *)stream, sampleCount);
	return sampleCount*4;
}

void PsxSpuFran::sl() {
	emsl.begin("spu");
	emsl.array("regArea", regArea, sizeof(regArea));
	emsl.array("mem", spuMem, sizeof(spuMem));
	emsl.var("addr", spuAddr);
	if (!emsl.save) {
		for (int i = 0; i < 0x100; i++) {
			if (i != H_SPUon1-0xc00 && i != H_SPUon2-0xc00)
				spuFranWriteRegister(0x1f801c00+i*2,regArea[i]);
		}
		spuFranWriteRegister(H_SPUon1,regArea[(H_SPUon1-0xc00)/2]);
		spuFranWriteRegister(H_SPUon2,regArea[(H_SPUon2-0xc00)/2]);
	}
	emsl.end();
}
