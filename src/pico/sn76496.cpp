/***************************************************************************

  sn76496.c

  Routines to emulate the Texas Instruments SN76489 / SN76496 programmable
  tone /noise generator. Also known as (or at least compatible with) TMS9919.

  Noise emulation is not accurate due to lack of documentation. The noise
  generator uses a shift register with a XOR-feedback network, but the exact
  layout is unknown. It can be set for either period or white noise; again,
  the details are unknown.

  28/03/2005 : Sebastien Chevalier
  Update th sn76496Write func, according to SN76489 doc found on SMSPower.
   - On write with 0x80 set to 0, when LastRegister is other then TONE,
   the function is similar than update with 0x80 set to 1
***************************************************************************/

#include "sn76496.h"
#include <base/emu.h>

#define MAX_OUTPUT 0x47ff // was 0x7fff

#define STEP 0x10000


/* Formulas for noise generator */
/* bit0 = output */

/* noise feedback for white noise mode (verified on real SN76489 by John Kortink) */
#define FB_WNOISE 0x14002	/* (16bits) bit16 = bit0(out) ^ bit2 ^ bit15 */

/* noise feedback for periodic noise mode */
//#define FB_PNOISE 0x10000 /* 16bit rorate */
#define FB_PNOISE 0x08000   /* JH 981127 - fixes Do Run Run */

/*
0x08000 is definitely wrong. The Master System conversion of Marble Madness
uses periodic noise as a baseline. With a 15-bit rotate, the bassline is
out of tune.
The 16-bit rotate has been confirmed against a real PAL Sega Master System 2.
Hope that helps the System E stuff, more news on the PSG as and when!
*/

/* noise generator start preset (for periodic noise) */
#define NG_PRESET 0x0f35

static const int SampleRate = 44100;

SN76496 sn76496;

void sn76496Write(int data)
{
	sn76496.write(data);
}

void SN76496::write(int data)
{
	int n, r;

	if (data & 0x80) {
		r = (data & 0x70) >> 4;
		lastReg = r;
		regs[r] = (regs[r] & 0x3F0) | (data & 0x0F);
	} else {
		r = lastReg;
		if ((r & 1) || (r == 6))
			regs[r] = (regs[r] & 0x3F0) | (data & 0x0F);
		else
			regs[r] = (regs[r] & 0x00F) | ((data & 0x3F) << 4);
	}

	int c = r/2;

	switch (r) {
	case 0:	/* tone 0 : frequency */
	case 2:	/* tone 1 : frequency */
	case 4:	/* tone 2 : frequency */
		period[c] = updateStep * regs[r];
		if (period[c] == 0)
			period[c] = updateStep;
		if (r == 4) {
			/* update noise shift frequency */
			if ((regs[6] & 0x03) == 0x03)
				period[3] = 2 * period[2];
		}
		break;
	case 1:	/* tone 0 : volume */
	case 3:	/* tone 1 : volume */
	case 5:	/* tone 2 : volume */
	case 7:	/* noise  : volume */
		volume[c] = volTable[data & 0x0f];
		break;
	case 6:	/* noise  : frequency, mode */
		n = regs[6];
		noiseFB = (n & 4) ? FB_WNOISE : FB_PNOISE;
		n &= 3;
		/* N/512,N/1024,N/2048,Tone #3 output */
		period[3] = ((n&3) == 3) ? 2 * period[2] : (updateStep << (5+(n&3)));

		/* reset noise shifter */
		noise = NG_PRESET;
		output[3] = noise & 1;
		break;
	}
}

void SN76496::update(int *buffer, int length)
{
	/* If the volume is 0, increase the counter */
	for (int i = 0; i < 4; i++) {
		if (volume[i] == 0) {
			/* note that I do count += length, NOT count = length + 1. You might think */
			/* it's the same since the volume is 0, but doing the latter could cause */
			/* interferencies when the program is rapidly modulating the volume. */
			if (count[i] <= length*STEP)
				count[i] += length*STEP;
		}
	}

	for (; length > 0; length--) {
		int vol[4];
		/* vol[] keeps track of how long each square wave stays */
		/* in the 1 position during the sample period. */
		vol[0] = vol[1] = vol[2] = vol[3] = 0;

		for (int i = 0; i < 3; i++) {
			if (output[i])
				vol[i] += count[i];
			count[i] -= STEP;
			/* Period[i] is the half period of the square wave. Here, in each */
			/* loop I add Period[i] twice, so that at the end of the loop the */
			/* square wave is in the same status (0 or 1) it was at the start. */
			/* vol[i] is also incremented by Period[i], since the wave has been 1 */
			/* exactly half of the time, regardless of the initial position. */
			/* If we exit the loop in the middle, Output[i] has to be inverted */
			/* and vol[i] incremented only if the exit status of the square */
			/* wave is 1. */
			while (count[i] <= 0) {
				count[i] += period[i];
				if (count[i] > 0) {
					output[i] ^= 1;
					if (output[i])
						vol[i] += period[i];
					break;
				}
				count[i] += period[i];
				vol[i] += period[i];
			}
			if (output[i])
				vol[i] -= count[i];
		}

		int left = STEP;
		do {
			int nextevent;

			if (count[3] < left)
				nextevent = count[3];
			else
				nextevent = left;

			if (output[3])
				vol[3] += count[3];
			count[3] -= nextevent;
			if (count[3] <= 0) {
				if (noise & 1)
					noise ^= noiseFB;
				noise >>= 1;
				output[3] = noise & 1;
				count[3] += period[3];
				if (output[3])
					vol[3] += period[3];
			}
			if (output[3])
				vol[3] -= count[3];

			left -= nextevent;
		} while (left > 0);

		int out = vol[0] * volume[0] + vol[1] * volume[1] +
				vol[2] * volume[2] + vol[3] * volume[3];

		if (out > MAX_OUTPUT * STEP)
			out = MAX_OUTPUT * STEP;

		if ((out /= STEP)) {// will be optimized to shift; max 0x47ff = 18431
			buffer[0] += out;
			buffer[1] += out;
		}
		buffer += 2;
	}
}

void SN76496::setClock(int clock)
{

	/* the base clock for the tone generators is the chip clock divided by 16; */
	/* for the noise generator, it is clock / 256. */
	/* Here we calculate the number of steps which happen during one sample */
	/* at the given sample rate. No. of events = sample rate / (clock/16). */
	/* STEP is a multiplier used to turn the fraction into a fixed point */
	/* number. */
	updateStep = ((qreal)STEP * SampleRate * 16) / clock;
}

void SN76496::setGain(int gain)
{
	gain &= 0xff;

	/* increase max output basing on gain (0.2 dB per step) */
	qreal out = MAX_OUTPUT / 3;
	for (; gain > 0; gain--)
		out *= 1.023292992;	/* = (10 ^ (0.2/20)) */

	/* build volume table (2dB per step) */
	for (int i = 0; i < 15; i++) {
		/* limit volume to avoid clipping */
		if (out > MAX_OUTPUT / 3)
			volTable[i] = MAX_OUTPUT / 3;
		else
			volTable[i] = out;

		out /= 1.258925412;	/* = 10 ^ (2/20) = 2dB */
	}
	volTable[15] = 0;
}

void SN76496::init(int clock)
{
	setClock(clock);

	for (int i = 0; i < 4; i++)
		volume[i] = 0;

	lastReg = 0;
	for (int i = 0; i < 8; i+=2) {
		regs[i] = 0;
		regs[i + 1] = 0x0F;	/* volume = 0 */
	}

	for (int i = 0; i < 4; i++) {
		output[i] = 0;
		period[i] = count[i] = updateStep;
	}
	noise = NG_PRESET;
	output[3] = noise & 1;

	setGain(0);
}

void SN76496::sl(const QString &name)
{
	emsl.array(name, regs, 28*4);
}
