/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2004 by Dave
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"
#include "cd_pcm.h"
#include "ym2612.h"
#include "sn76496.h"
#include <QtEndian>

static const int PicoSoundSampleRate = 44100;
int picoSoundLen = 0; // number of mono samples, multiply by 2 for stereo
bool picoSoundEnabled = true;

static const int PicoSoundOutBufferSize = 8192;
static u32 picoSoundOutBuffer[PicoSoundOutBufferSize];
static int picoSoundOutHead = 0;
static int picoSoundOutTail = 0;

static int picoSoundMixBuffer[2*44100/50]__attribute__((aligned(4))); // TODO needed ??

static u16 picoSoundDacInfo[312]; // pppppppp ppppllll, p - pos in buff, l - length to write for this sample

static void picoSoundFinishMixing(int offset, int count)
{
	int *src = picoSoundMixBuffer + offset;
	u32 *dst = picoSoundOutBuffer;
	int l, r;
	for (; count > 0; count--) {
		l = qBound(S32_MIN, *src++, S32_MAX);
		r = qBound(S32_MIN, *src++, S32_MAX);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		u32 sample = l | (r << 16);
#else
		u32 sample = (l << 16) | r;
#endif
		dst[picoSoundOutHead] = sample;
		picoSoundOutHead++;
		picoSoundOutHead &= PicoSoundOutBufferSize-1;
	}
}

int PicoEmu::fillAudioBuffer(char *stream, int streamSize)
{
	u32 *src = picoSoundOutBuffer;
	u32 *dst = (u32 *)stream;
	int available = (picoSoundOutHead-picoSoundOutTail+PicoSoundOutBufferSize) & (PicoSoundOutBufferSize-1);
	int length = qMin(available, streamSize>>2);

	if ((picoSoundOutTail+length) >= PicoSoundOutBufferSize) {
		u32 partialLength = PicoSoundOutBufferSize - picoSoundOutTail;
		memcpy(dst, src+picoSoundOutTail, partialLength<<2);
		// picoSoundOutTail = 0;
		u32 remainderLength = length - partialLength;
		memcpy(dst+partialLength, src, remainderLength<<2);
		picoSoundOutTail = remainderLength;
	} else {
		memcpy(dst, src+picoSoundOutTail, length<<2);
		picoSoundOutTail += length;
	}
	return length<<2;
}

static void picoSoundDacCalculateShrink(int lines, int mid)
{
	Q_UNUSED(mid)
	int dacCounter = -picoSoundLen;
	int len = 1;
	int pos = 0;
	picoSoundDacInfo[225] = 1;

	for (int i = 226; i != 225; i++) {
		if (i >= lines)
			i = 0;
		len = 0;
		if (dacCounter < 0) {
			len = 1;
			pos++;
			dacCounter += lines;
		}
		dacCounter -= picoSoundLen;
		picoSoundDacInfo[i] = (pos<<4)|len;
	}
}

static void picoSoundDacCalculateStretch(int lines, int mid)
{
	int dacCounter = picoSoundLen;
	int pos = 0;
	for (int i = 225; i != 224; i++) {
		if (i >= lines)
			i = 0;
		int len = 0;
		while (dacCounter >= 0) {
			dacCounter -= lines;
			len++;
		}
		if (i == mid) { // midpoint
			while (pos+len < picoSoundLen/2) {
				dacCounter -= lines;
				len++;
			}
		}
		dacCounter += picoSoundLen;
		picoSoundDacInfo[i] = (pos<<4)|len;
		pos += len;
	}
	int len = picoSoundLen - pos;
	picoSoundDacInfo[224] = (pos<<4)|len;
}

static void picoSoundDacCalculate()
{
	int lines = Pico.m.pal ? 312 : 262;
	int mid = Pico.m.pal ? 68 : 93;
	if (picoSoundLen <= lines)
		picoSoundDacCalculateShrink(lines, mid);
	else
		picoSoundDacCalculateStretch(lines, mid);
}

void picoSoundReset()
{
	z80startCycle = z80stopCycle = 0;

	int osc = Pico.m.pal ? OSC_PAL : OSC_NTSC;
	YM2612Init(osc/7, PicoSoundSampleRate);
	sn76496.init(osc/15);

	// calculate picoSoundLen
	int targetFps = Pico.m.pal ? 50 : 60;
	picoSoundLen = PicoSoundSampleRate / targetFps;

	// recalculate dac info
	picoSoundDacCalculate();

	if (PicoMCD & 1)
		picoMcdPcmSetRate(PicoSoundSampleRate);

	// clear mix buffer
	memset32(picoSoundMixBuffer, 0, sizeof(picoSoundMixBuffer)/4);
}

void picoSoundTimersAndDac(int raster)
{
	// our raster lasts 63.61323/64.102564 microseconds (NTSC/PAL)
	YM2612PicoTick(1);

	bool doDac = picoSoundEnabled && (PicoOpt&1) && *ym2612_dacen;
	if (!doDac)
		return;

	int pos = picoSoundDacInfo[raster];
	int len = pos & 0xf;
	if (!len)
		return;

	pos >>= 4;

	int *d = picoSoundMixBuffer + pos*2;
	int dacOut = *ym2612_dacout;
	// some manual loop unrolling here :)
	d[0] = dacOut;
	if (len > 1) {
		d[2] = dacOut;
		if (len > 2)
			d[4] = dacOut;
	}
}

int picoSoundRender(int offset, int length)
{
	int *buf32 = picoSoundMixBuffer + offset;

	if (PicoOpt & 2) // PSG
		sn76496.update(buf32, length);
	if (PicoOpt & 1) // FM
		YM2612Update(buf32, length);
	// emulating CD && PCM option enabled && PCM chip on && have enabled channels
	int doPcm = (PicoMCD&1) && (PicoOpt&0x400) && (Pico_mcd->pcm.control & 0x80) && Pico_mcd->pcm.enabled;
	if (doPcm)// CD: PCM sound
		picoMcdPcmUpdate(buf32, length);

	picoSoundFinishMixing(offset, length);
	memset32(buf32, 0, length<<1);
	return length;
}
