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

#include <base/emu.h>
#include "amiga.h"
#include "mem.h"
#include "custom.h"
#include "cpu.h"
#include "events.h"
#include "spu.h"

#define PERIOD_MAX ULONG_MAX
static const int SoundFrequency = 44100;
static const int SoundEvtimePal = MAXHPOS_PAL*313*VBLANK_HZ_PAL*CYCLE_UNIT/SoundFrequency;

class SpuChannelData {
public:
	void sl(int i);

	u32 per;
	u16 len;
	bool dmaEnabled;
	bool intreq2;
	bool dataWritten;
	u32 lc;
	u32 pt;
	int wper;
	u16 wlen;
	u16 dat;
	u16 nextdat;
};

static SpuChannelData spuChannel[4];
static int spuChannelCurrentSample[4];
static int spuChannelVolume[4];
static u32 spuChannelAdkMask[4];
static int spuChannelState[4];
static u32 spuChannelEvtime[4];

static u32 spuScaledSampleEvtime;
static u32 spuLastCycles;
static u32 spuNextSampleEvtime;

static const int SoundBufferSize = 8192;
static u16 soundQueue[SoundBufferSize];
static int soundHead = 0;
static int soundTail = 0;

void amigaSpuDefaultEvtime() {
	spuScaledSampleEvtime = SoundEvtimePal;
	amigaSpuSchedule();
}

int amigaSpuFillAudioBuffer(char *stream, int length) {
	int sampleLength = (soundHead - soundTail + SoundBufferSize) % SoundBufferSize;
	if (sampleLength > length / 2)
		sampleLength = length / 2;
	if (!sampleLength)
		return 0;
	if ((soundTail + sampleLength) >= SoundBufferSize) {
		u32 partialLength = SoundBufferSize - soundTail;
		memcpy(stream, soundQueue + soundTail, partialLength*2);
		soundTail = 0;
		u32 remainderLength = sampleLength - partialLength;
		memcpy(stream + partialLength*2, soundQueue, remainderLength*2);
		soundTail = remainderLength;
	} else {
		memcpy(stream, soundQueue + soundTail, sampleLength*2);
		soundTail += sampleLength;
	}
	return sampleLength*2;
}

static inline void soundQueueSample(u16 sample) {
	soundQueue[soundHead] = sample;
	soundHead = (soundHead+1) % SoundBufferSize;
}

static inline void spuSampleHandler() {
	int data[4];
	for (int i = 0; i < 4; i++) {
		data[i] = spuChannelCurrentSample[i] * spuChannelVolume[i];
		data[i] &= spuChannelAdkMask[i];
	}
	soundQueueSample(data[0]+data[2]);
	soundQueueSample(data[1]+data[3]);
}

void amigaSpuSchedule() {
	amigaEvents[AmigaEventSpu].active = 0;

	u32 best = ~0ul;
	for (int i = 0; i < 4; i++) {
		if (spuChannelState[i]) {
			if (best > spuChannelEvtime[i]) {
				best = spuChannelEvtime[i];
				amigaEvents[AmigaEventSpu].active = 1;
			}
		}
	}
	amigaEvents[AmigaEventSpu].time = amigaCycles + best;
}

static inline void spuHandler(int nr) {
	SpuChannelData *ch = spuChannel + nr;
	bool audav = adkcon & (1 << nr);
	bool audap = adkcon & (0x10 << nr);
	bool napnav = (!audav && !audap) || audav;
	switch (spuChannelState[nr]) {
	case 0:
		qDebug("Bug in sound code\n");
		break;
	case 1:
		spuChannelEvtime[nr] = maxhpos * CYCLE_UNIT;
		spuChannelState[nr] = 5;
		INTREQ(0x8000 | (0x80 << nr));
		if (ch->wlen != 1)
			ch->wlen--;
		ch->nextdat = amigaMemChipGetWord(ch->pt);
		ch->pt += 2;
		break;
	case 5:
		spuChannelEvtime[nr] = ch->per;
		ch->dat = ch->nextdat;
		spuChannelCurrentSample[nr] = (s8)((ch->dat >> 8) & 0xFF);
		spuChannelState[nr] = 2;
		ch->dataWritten |= napnav;
		break;
	case 2:
		spuChannelCurrentSample[nr] = (s8)((ch->dat >> 0) & 0xFF);
		spuChannelEvtime[nr] = ch->per;
		spuChannelState[nr] = 3;
		if (audap) {
			if (ch->intreq2 && ch->dmaEnabled)
				INTREQ (0x8000 | (0x80 << nr));
			ch->intreq2 = false;
			ch->dat = ch->nextdat;
			ch->dataWritten |= ch->dmaEnabled;
			if (nr < 3) {
				if (ch->dat == 0)
					(ch+1)->per = PERIOD_MAX;
				else if (ch->dat < maxhpos * CYCLE_UNIT / 2)
					(ch+1)->per = maxhpos * CYCLE_UNIT / 2;
				else
					(ch+1)->per = ch->dat * CYCLE_UNIT;
			}
		}
		break;
	case 3:
		spuChannelEvtime[nr] = ch->per;
		if ((INTREQR() & (0x80 << nr)) && !ch->dmaEnabled) {
			spuChannelState[nr] = 0;
			spuChannelCurrentSample[nr] = 0;
			break;
		} else {
			spuChannelState[nr] = 2;
			if ((ch->intreq2 && ch->dmaEnabled && napnav) || (napnav && !ch->dmaEnabled))
				INTREQ(0x8000 | (0x80 << nr));
			ch->intreq2 = false;
			ch->dat = ch->nextdat;
			spuChannelCurrentSample[nr] = (s8)((ch->dat >> 8) & 0xFF);
			if (ch->dmaEnabled && napnav)
				ch->dataWritten |= (ch->dmaEnabled && napnav);
			if (audav && nr < 3)
				spuChannelVolume[nr+1] = ch->dat;
		}
		break;
	default:
		spuChannelState[nr] = 0;
		break;
	}
}

void amigaSpuReset() {
	memset(spuChannel, 0, 4 * sizeof(*spuChannel));
	for (int i = 0; i < 4; i++)
		spuChannel[i].per = PERIOD_MAX;
	spuLastCycles = 0;
	spuNextSampleEvtime = spuScaledSampleEvtime;
	amigaSpuSchedule();
}

void amigaSpuUpdate() {
	u32 nCycles = amigaCycles - spuLastCycles;
	for (;;) {
		register u32 bestEvtime = nCycles + 1;
		for (int i = 0; i < 4; i++) {
			if (spuChannelState[i] && bestEvtime > spuChannelEvtime[i])
				bestEvtime = spuChannelEvtime[i];
		}
		if (bestEvtime > spuNextSampleEvtime)
			bestEvtime = spuNextSampleEvtime;
		if (bestEvtime > nCycles)
			break;
		spuNextSampleEvtime -= bestEvtime;
		for (int i = 0; i < 4; i++)
			spuChannelEvtime[i] -= bestEvtime;
		nCycles -= bestEvtime;
		if (!spuNextSampleEvtime) {
			spuNextSampleEvtime = spuScaledSampleEvtime;
			spuSampleHandler();
		}
		for (int i = 0; i < 4; i++) {
			if (!spuChannelEvtime[i] && spuChannelState[i])
				spuHandler(i);
		}
	}
	spuLastCycles = amigaCycles - nCycles;
}

void amigaSpuHandler() {
	amigaSpuUpdate();
	amigaSpuSchedule();
}

void AUDxDAT(int nr, u16 v) {
	SpuChannelData *ch = spuChannel + nr;
	amigaSpuUpdate();
	ch->dat = v;
	if (spuChannelState[nr] == 0 && !(INTREQR() & (0x80 << nr))) {
		spuChannelState[nr] = 2;
		INTREQ(0x8000 | (0x80 << nr));
		spuChannelEvtime[nr] = ch->per;
		amigaSpuSchedule();
		amigaEventsSchedule();
	}
}

void AUDxLCH(int nr, u16 v) {
	amigaSpuUpdate();
	spuChannel[nr].lc = (spuChannel[nr].lc & 0xFFFF) | ((u32)v << 16);
}

void AUDxLCL(int nr, u16 v) {
	amigaSpuUpdate();
	spuChannel[nr].lc = (spuChannel[nr].lc & ~0xFFFF) | (v & 0xFFFE);
}

void AUDxPER(int nr, u16 v) {
	u32 per = v * CYCLE_UNIT;
	amigaSpuUpdate ();
	if (per == 0)
		per = PERIOD_MAX;
	if (per < maxhpos * CYCLE_UNIT / 2)
		per = maxhpos * CYCLE_UNIT / 2;
	if (spuChannel[nr].per == PERIOD_MAX && per != PERIOD_MAX) {
		spuChannelEvtime[nr] = CYCLE_UNIT;
		amigaSpuSchedule();
		amigaEventsSchedule();
	}
	spuChannel[nr].per = per;
}

void AUDxLEN(int nr, u16 v) {
	amigaSpuUpdate();
	spuChannel[nr].len = v;
}

void AUDxVOL(int nr, u16 v) {
	amigaSpuUpdate();
	spuChannelVolume[nr] = ((v & 64) ? 63 : (v & 63));
}

static inline void spuChannelEnableDma(int i) {
	SpuChannelData *ch = &spuChannel[i];
	if (spuChannelState[i] == 0) {
		spuChannelState[i] = 1;
		ch->pt = ch->lc;
		ch->wper = ch->per;
		ch->wlen = ch->len;
		ch->dataWritten = true;
		spuChannelEvtime[i] = amigaEvents[AmigaEventHSync].time - amigaCycles;
	}
}

static inline void spuChannelDisableDma(int i) {
	if (spuChannelState[i] == 1 || spuChannelState[i] == 5) {
		spuChannelState[i] = 0;
		spuChannelCurrentSample[i] = 0;
	}
}

void amigaSpuCheckDma() {
	for(int i = 0; i < 4; i++) {
		SpuChannelData *ch = &spuChannel[i];
		bool dmaEnabled = (dmacon & 0x200) && (dmacon & (1<<i));
		if (ch->dmaEnabled != dmaEnabled) {
			ch->dmaEnabled = dmaEnabled;
			if (ch->dmaEnabled)
				spuChannelEnableDma(i);
			else
				spuChannelDisableDma(i);
		}
	}
}

void amigaSpuFetchAudio() {
	for (int i = 0; i < 4; i++) {
		SpuChannelData *ch = &spuChannel[i];
		if (ch->dataWritten) {
			ch->dataWritten = false;
			ch->nextdat = amigaMemChipGetWord(ch->pt);
			ch->pt += 2;
			if (spuChannelState[i] == 2 || spuChannelState[i] == 3) {
				if (ch->wlen == 1) {
					ch->pt = ch->lc;
					ch->wlen = ch->len;
					ch->intreq2 = true;
				} else {
					ch->wlen--;
				}
			}
		}
	}
}

void amigaSpuUpdateAdkMasks() {
	u32 t = adkcon | (adkcon >> 4);
	spuChannelAdkMask[0] = (((t >> 0) & 1) - 1);
	spuChannelAdkMask[1] = (((t >> 1) & 1) - 1);
	spuChannelAdkMask[2] = (((t >> 2) & 1) - 1);
	spuChannelAdkMask[3] = (((t >> 3) & 1) - 1);
}

void amigaSpuSl() {
	emsl.begin("spu");
	emsl.var("scaledSampleEvtime", spuScaledSampleEvtime);
	emsl.var("lastCycles", spuLastCycles);
	emsl.var("nextSampleEvtime", spuNextSampleEvtime);
	emsl.end();

	for (int i = 0; i < 4; i++)
		spuChannel[i].sl(i);
	if (!emsl.save)
		soundHead = soundTail = 0;
}

void SpuChannelData::sl(int i) {
	emsl.begin(QString("spu ch%1").arg(i));
	emsl.var("per", per);
	emsl.var("len", len);
	emsl.var("dmaEn", dmaEnabled);
	emsl.var("intreq2", intreq2);
	emsl.var("dataWritten", dataWritten);
	emsl.var("lc", lc);
	emsl.var("pt", pt);
	emsl.var("wper", wper);
	emsl.var("wlen", wlen);
	emsl.var("dat", dat);
	emsl.var("nextdat", nextdat);
	emsl.var("currentSample", spuChannelCurrentSample[i]);
	emsl.var("vol", spuChannelVolume[i]);
	emsl.var("adkMask", spuChannelAdkMask[i]);
	emsl.var("state", spuChannelState[i]);
	emsl.var("evtime", spuChannelEvtime[i]);
	emsl.end();
}
