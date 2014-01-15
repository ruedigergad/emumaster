/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#ifndef PICOEMU_H
#define PICOEMU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <base/emu.h>
#include "cd_sys.h"

extern u8 *picoRom;
extern uint picoRomSize;

enum PicoFlag {
	StereoSound			= 0x0008
};

extern int picoFlags;

enum PicoMcdOpt {
	PicoMcdEnabled		= 0x0001
};

enum PicoRegion {
	PicoRegionAuto		= 0,
	PicoRegionJapanNtsc	= 1,
	PicoRegionJapanPal	= 2,
	PicoRegionUsa		= 4,
	PicoRegionEurope	= 8
};

extern int picoMcdOpt;

extern void picoScanLine(uint num);

#define picoDebugSek(...)
#define picoDebugMcdSek(...)
#define picoDebugMcdToc(...)

extern const unsigned char  hcounts_32[];
extern const unsigned char  hcounts_40[];
extern const unsigned short vcounts[];

// port-specific compile-time settings
#include "port_config.h"

#ifdef __cplusplus

class Mp3Player;

class PicoEmu : public Emu {
	Q_OBJECT
public:
	PicoEmu();
	bool init(const QString &diskPath, QString *error);
	void shutdown();
	void reset();

	void emulateFrame(bool drawEnabled);
	const QImage &frame() const;
	int fillAudioBuffer(char *stream, int streamSize);
	Mp3Player *mp3Player() const;
protected:
	void sl();
	void pause();
	void resume();
	void updateInput();
private:
	bool findMcdBios(QString *biosFileName, QString *error);

	int m_lastVideoMode;
	Mp3Player *m_mp3Player;
};

inline Mp3Player *PicoEmu::mp3Player() const
{ return m_mp3Player; }

extern PicoEmu picoEmu;

#endif

#ifdef __cplusplus
extern "C" {
#endif

// Pico.c
// PicoOpt bits LSb->MSb:
// 1 enable_ym2612&dac
// 2 enable_sn76496
// 4 enable_z80
// 8 stereo_sound,
// alt_renderer, 6button_gamepad, accurate_timing, accurate_sprites,
// draw_no_32col_border, external_ym2612, enable_cd_pcm, enable_cd_cdda
// enable_cd_gfx, cd_perfect_sync, soft_32col_scaling, enable_cd_ramcart
// disable_vdp_fifo
extern int PicoOpt;
extern int PicoSkipFrame; // skip rendering frame, but still do sound (if enabled) and emulation stuff
extern int PicoRegionOverride; // override the region detection 0: auto, 1: Japan NTSC, 2: Japan PAL, 4: US, 8: Europe
extern int PicoAutoRgnOrder; // packed priority list of regions, for example 0x148 means this detection order: EUR, USA, JAP
int PicoInit(void);
void PicoExit(void);
int PicoReset(int hard);
int PicoFrame(void);
void PicoFrameDrawOnly(void);
extern int PicoPad[2]; // Joypads, format is MXYZ SACB RLDU
#define PicoMessage qDebug

unsigned int PicoRead16(unsigned int a);
void PicoWrite16(unsigned int a, unsigned short d);

// cd/Pico.c
extern void (*PicoMCDopenTray)(void);
extern int  (*PicoMCDcloseTray)(void);

// cd/cd_sys.c
bool Insert_CD(const QString &fileName, QString *error);
void Stop_CD(void); // releases all resources taken when CD game was started.

// Draw.c
extern void *DrawLineDest;

// internals
extern unsigned short HighPal[0x100];
extern int rendstatus;
// utility
#ifdef _ASM_DRAW_C
void *blockcpy(void *dst, const void *src, size_t n);
void vidConvCpyRGB565(void *to, void *from, int pixels);
#else
#define blockcpy memcpy
#endif

// Draw2.c
// stuff below is optional
extern unsigned char  *PicoDraw2FB;  // buffer for fasr renderer in format (8+320)x(8+224+8) (eights for borders)
extern unsigned short *PicoCramHigh; // pointer to CRAM buff (0x40 shorts), converted to native device color (works only with 16bit for now)
extern void (*PicoPrepareCram)();    // prepares PicoCramHigh for renderer to use

// sound.cpp
extern int picoSoundLen;
extern bool picoSoundEnabled;

#ifdef __cplusplus
} // End of extern "C"
#endif

#define USE_POLL_DETECT

// to select core, define EMU_C68K, EMU_M68K or EMU_F68K in your makefile or project

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------- 68000 CPU -----------------------
#include "cyclone.h"
extern struct Cyclone PicoCpuCM68k, PicoCpuCS68k;
#define SekCyclesLeftNoMCD PicoCpuCM68k.cycles // cycles left for this run
#define SekCyclesLeft \
	(((PicoMCD&1) && (PicoOpt & 0x2000)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & 0x2000) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuCS68k.cycles)
#define SekSetCyclesLeftNoMCD(c) PicoCpuCM68k.cycles=c
#define SekSetCyclesLeft(c) { \
	if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCycleCnt=SekCycleAim-(c); else SekSetCyclesLeftNoMCD(c); \
}
#define SekPc (PicoCpuCM68k.pc-PicoCpuCM68k.membase)
#define SekPcS68k (PicoCpuCS68k.pc-PicoCpuCS68k.membase)
#define SekSetStop(x) { PicoCpuCM68k.state_flags&=~1; if (x) { PicoCpuCM68k.state_flags|=1; PicoCpuCM68k.cycles=0; } }
#define SekSetStopS68k(x) { PicoCpuCS68k.state_flags&=~1; if (x) { PicoCpuCS68k.state_flags|=1; PicoCpuCS68k.cycles=0; } }
#define SekIsStoppedS68k() (PicoCpuCS68k.state_flags&1)
#define SekShouldInterrupt (PicoCpuCM68k.irq > (PicoCpuCM68k.srh&7))

#define SekInterrupt(i) PicoCpuCM68k.irq=i

extern int SekCycleCnt; // cycles done in this frame
extern int SekCycleAim; // cycle aim
extern unsigned int SekCycleCntT; // total cycle counter, updated once per frame

#define SekCyclesReset() { \
	SekCycleCntT+=SekCycleAim; \
	SekCycleCnt-=SekCycleAim; \
	SekCycleAim=0; \
}
#define SekCyclesBurn(c)  SekCycleCnt+=c
#define SekCyclesDone()  (SekCycleAim-SekCyclesLeft)    // nuber of cycles done in this frame (can be checked anywhere)
#define SekCyclesDoneT() (SekCycleCntT+SekCyclesDone()) // total nuber of cycles done for this rom

#define SekEndRun(after) { \
	SekCycleCnt -= SekCyclesLeft - after; \
	if(SekCycleCnt < 0) SekCycleCnt = 0; \
	SekSetCyclesLeft(after); \
}

extern int SekCycleCntS68k;
extern int SekCycleAimS68k;

#define SekCyclesResetS68k() { \
	SekCycleCntS68k-=SekCycleAimS68k; \
	SekCycleAimS68k=0; \
}
#define SekCyclesDoneS68k()  (SekCycleAimS68k-SekCyclesLeftS68k)

// ----------------------- Z80 CPU -----------------------

#include "drz80.h"

extern struct DrZ80 drZ80;

#define z80_run(cycles)    ((cycles) - DrZ80Run(&drZ80, cycles))
#define z80_run_nr(cycles) DrZ80Run(&drZ80, cycles)
#define z80_int() { \
  drZ80.z80irqvector = 0xFF; /* default IRQ vector RST opcode */ \
  drZ80.Z80_IRQ = 1; \
}
#define z80_resetCycles()

// ---------------------------------------------------------

extern int PicoMCD;

// main oscillator clock which controls timing
#define OSC_NTSC 53693100
// seems to be accurate, see scans from http://www.hot.ee/tmeeco/
#define OSC_PAL  53203424

struct PicoVideo
{
  unsigned char reg[0x20];
  unsigned int command;       // 32-bit Command
  unsigned char pending;      // 1 if waiting for second half of 32-bit command
  unsigned char type;         // Command type (v/c/vsram read/write)
  unsigned short addr;        // Read/Write address
  int status;                 // Status bits
  unsigned char pending_ints; // pending interrupts: ??VH????
  signed char lwrite_cnt;     // VDP write count during active display line
  unsigned char pad[0x12];
};

struct PicoMisc
{
  unsigned char rotate;
  unsigned char z80Run;
  unsigned char padTHPhase[2]; // 02 phase of gamepad TH switches
  short scanline;              // 04 0 to 261||311; -1 in fast mode
  char dirtyPal;               // 06 Is the palette dirty (1 - change @ this frame, 2 - some time before)
  unsigned char hardware;      // 07 Hardware value for country
  unsigned char pal;           // 08 1=PAL 0=NTSC
  unsigned char sram_reg;      // SRAM mode register. bit0: allow read? bit1: deny write? bit2: EEPROM? bit4: detected? (header or by access)
  unsigned short z80_bank68k;  // 0a
  unsigned short z80_lastaddr; // this is for Z80 faking
  unsigned char  z80_fakeval;
  unsigned char  pad0;
  unsigned char  padDelay[2];  // 10 gamepad phase time outs, so we count a delay
  unsigned short eeprom_addr;  // EEPROM address register
  unsigned char  eeprom_cycle; // EEPROM SRAM cycle number
  unsigned char  eeprom_slave; // EEPROM slave word for X24C02 and better SRAMs
  unsigned char prot_bytes[2]; // simple protection faking
  unsigned short dma_xfers;
  unsigned char pad[2];
  unsigned int  frame_count; // TODO unused but asm code has hardcoded offsets
};

// some assembly stuff depend on these, do not touch!
struct Pico
{
  unsigned char ram[0x10000];  // 0x00000 scratch ram
  unsigned short vram[0x8000]; // 0x10000
  unsigned char zram[0x2000];  // 0x20000 Z80 ram
  unsigned char ioports[0x10];
  unsigned int pad[0x3c];      // unused
  unsigned short cram[0x40];   // 0x22100
  unsigned short vsram[0x40];  // 0x22180

  unsigned char *rom;          // 0x22200
  unsigned int romsize;        // 0x22204

  struct PicoMisc m;
  struct PicoVideo video;
};

// sram
struct PicoSRAM
{
  unsigned char *data;		// actual data
  unsigned int start;		// start address in 68k address space
  unsigned int end;
  unsigned char unused1;	// 0c: unused
  unsigned char unused2;
  unsigned char changed;
  unsigned char eeprom_type;    // eeprom type: 0: 7bit (24C01), 2: device with 2 addr words (X24C02+), 3: dev with 3 addr words
  unsigned char eeprom_abits;	// eeprom access must be odd addr for: bit0 ~ cl, bit1 ~ out
  unsigned char eeprom_bit_cl;	// bit number for cl
  unsigned char eeprom_bit_in;  // bit number for in
  unsigned char eeprom_bit_out; // bit number for out
};

// MCD
#include "lc89510.h"
#include "cd_gfx.h"

struct pcm_chan			// 08, size 0x10
{
	unsigned char regs[8];
	unsigned int  addr;	// .08: played sample address
	int pad;
};

struct mcd_pcm
{
	unsigned char control; // reg7
	unsigned char enabled; // reg8
	unsigned char cur_ch;
	unsigned char bank;
	int pad1;

	struct pcm_chan ch[8];
};

struct mcd_misc
{
	unsigned short hint_vector;
	unsigned char  busreq;
	unsigned char  s68k_pend_ints;
	unsigned int   state_flags;	// 04: emu state: reset_pending, dmna_pending
	unsigned int   counter75hz;
	unsigned short audio_offset;	// 0c: for savestates: play pointer offset (0-1023)
	unsigned char  audio_track;	// playing audio track # (zero based)
	char           pad1;
	int            timer_int3;	// 10
	unsigned int   timer_stopwatch;
	unsigned char  bcram_reg;	// 18: battery-backed RAM cart register
	unsigned char  pad2;
	unsigned short pad3;
	int pad[9];
};

typedef struct
{
	unsigned char bios[0x20000];			// 000000: 128K
	union {						// 020000: 512K
		unsigned char prg_ram[0x80000];
		unsigned char prg_ram_b[4][0x20000];
	};
	union {						// 0a0000: 256K
		struct {
			unsigned char word_ram2M[0x40000];
			unsigned char unused0[0x20000];
		};
		struct {
			unsigned char unused1[0x20000];
			unsigned char word_ram1M[2][0x20000];
		};
	};
	union {						// 100000: 64K
		unsigned char pcm_ram[0x10000];
		unsigned char pcm_ram_b[0x10][0x1000];
	};
	unsigned char s68k_regs[0x200];			// 110000: GA, not CPU regs
	unsigned char bram[0x2000];			// 110200: 8K
	struct mcd_misc m;				// 112200: misc
	struct mcd_pcm pcm;				// 112240:
	PicoMcdToc TOC;					// not to be saved
	PicoMcdScd scd;
	CDD  cdd;
	CDC  cdc;
	Rot_Comp rot_comp;
} mcd_state;

#define Pico_mcd ((mcd_state *)Pico.rom)

// Debug.c
int CM_compareRun(int cyc, int is_sub);

// Draw.c
int PicoLine(int scan);
void PicoFrameStart(void);

// Draw2.c
void PicoFrameFull();

// Memory.c
int PicoInitPc(unsigned int pc);
unsigned int PicoRead32(unsigned int a);
void PicoMemSetup(void);
void PicoMemReset(void);
int PadRead(int i);
unsigned char z80_read(unsigned short a);
void z80_write(unsigned char data, unsigned short a);
void z80_write16(unsigned short data, unsigned short a);
unsigned short z80_read16(unsigned short a);

// cd/Memory.c
void PicoMemSetupCD(void);
void PicoMemResetCD(int r3);
void PicoMemResetCDdecode(int r3);

// Pico.c
extern struct Pico Pico;
extern struct PicoSRAM SRam;
extern int emustatus;
extern int z80startCycle, z80stopCycle; // in 68k cycles
int CheckDMA(void);

// cd/Pico.c
int  PicoInitMCD(void);
void PicoExitMCD(void);
int PicoResetMCD(int hard);
int PicoFrameMCD(void);

// Sek.c
int SekInit(void);
int SekReset(void);
void SekState(int *data);
void SekSetRealTAS(int use_real);

// cd/Sek.c
void SekInitS68k(void);
void SekResetS68k();
int SekInterruptS68k(int irq);

// VideoPort.c
void PicoVideoWrite(unsigned int a,unsigned short d);
unsigned int PicoVideoRead(unsigned int a);

// Misc.c
void SRAMWriteEEPROM(unsigned int d);
void SRAMUpdPending(unsigned int a, unsigned int d);
unsigned int SRAMReadEEPROM(void);
void memcpy16(unsigned short *dest, unsigned short *src, int count);
void memcpy16bswap(unsigned short *dest, void *src, int count);
void memcpy32(int *dest, int *src, int count); // 32bit word count
void memset32(int *dest, int c, int count);

// cd/Misc.c
void wram_2M_to_1M(unsigned char *m);
void wram_1M_to_2M(unsigned char *m);

// cd/buffering.c
void PicoCDBufferRead(void *dest, int lba);

// sound/sound.c
void picoSoundReset();
void picoSoundTimersAndDac(int raster);
int  picoSoundRender(int offset, int length);
// z80 functionality wrappers
void z80_init(void);
void z80_reset(void);
void z80Sl(const QString &name);

#ifdef __cplusplus
} // End of extern "C"
#endif

// emulation event logging
#ifndef EL_LOGMASK
#define EL_LOGMASK 0
#endif

#define EL_HVCNT   0x0001 /* hv counter reads */
#define EL_SR      0x0002 /* SR reads */
#define EL_INTS    0x0004 /* ints and acks */
#define EL_YM2612R 0x0008 /* 68k ym2612 reads */
#define EL_INTSW   0x0010 /* log irq switching on/off */
#define EL_ASVDP   0x0020 /* VDP accesses during active scan */
#define EL_VDPDMA  0x0040 /* VDP DMA transfers and their timing */
#define EL_BUSREQ  0x0080 /* z80 busreq r/w or reset w */
#define EL_Z80BNK  0x0100 /* z80 i/o through bank area */
#define EL_SRAMIO  0x0200 /* sram i/o */
#define EL_EEPROM  0x0400 /* eeprom debug */
#define EL_UIO     0x0800 /* unmapped i/o */
#define EL_IO      0x1000 /* all i/o */
#define EL_CDPOLL  0x2000 /* MCD: log poll detection */

#define EL_STATUS  0x4000 /* status messages */
#define EL_ANOMALY 0x8000 /* some unexpected conditions (during emulation) */

#if EL_LOGMASK
#define lprintf qDebug
#define elprintf(w,f,...) \
{ \
	if ((w) & EL_LOGMASK) \
		lprintf("%03i: " f "\n",Pico.m.scanline,##__VA_ARGS__); \
}
#else
#define elprintf(w,f,...)
#endif

#endif // PICOEMU_H
