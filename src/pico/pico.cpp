/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	Original code (c) 2004 by Dave
	Original code (c) Copyright 2007, Grazvydas "notaz" Ignotas
	(c) Copyright 2011, elemental
*/

#include "pico.h"
#include "cart.h"
#include "mp3player.h"
#include "ym2612.h"
#include <base/pathmanager.h>
#include <base/emuview.h>
#include <QImage>
#include <QApplication>
#include <QFile>
#include <QDir>

u8 *picoRom = 0;
uint picoRomSize = 0;
int picoMcdOpt = 0;

struct Pico Pico;
int PicoOpt=0; // disable everything by default
int PicoSkipFrame=0; // skip rendering frame?
int PicoRegionOverride = 0; // override the region detection 0: Auto, 1: Japan NTSC, 2: Japan PAL, 4: US, 8: Europe
int PicoAutoRgnOrder = 0;
int emustatus = 0; // rapid_ym2612, multi_ym_updates

struct PicoSRAM SRam = {0,};
int z80startCycle, z80stopCycle; // in 68k cycles
int PicoPad[2];  // Joypads, format is SACB RLDU
int PicoMCD = 0; // mega CD status: scd_started

// to be called once on emu init
int PicoInit(void)
{
  // Blank space for state:
  memset(&Pico,0,sizeof(Pico));
  memset(&PicoPad,0,sizeof(PicoPad));

  // Init CPUs:
  SekInit();
  z80_init(); // init even if we aren't going to use it

  PicoInitMCD();

  SRam.data=0;

  return 0;
}

// to be called once on emu exit
void PicoExit(void)
{
  if (PicoMCD&1)
    PicoExitMCD();

  if(SRam.data) free(SRam.data); SRam.data=0;
}

int PicoReset(int hard)
{
  unsigned int region=0;
  int support=0,hw=0,i=0;
  unsigned char pal=0;
  unsigned char sram_reg=Pico.m.sram_reg; // must be preserved

  if (Pico.romsize<=0) return 1;

  PicoMemReset();
  SekReset();
  // s68k doesn't have the TAS quirk, so we just globally set normal TAS handler in MCD mode (used by Batman games).
  SekSetRealTAS(PicoMCD & 1);
  SekCycleCntT=0;
  z80_reset();

  // reset VDP state, VRAM and PicoMisc
  //memset(&Pico.video,0,sizeof(Pico.video));
  //memset(&Pico.vram,0,sizeof(Pico.vram));
  memset(Pico.ioports,0,sizeof(Pico.ioports)); // needed for MCD to reset properly
  memset(&Pico.m,0,sizeof(Pico.m));
  Pico.video.pending_ints=0;
  emustatus = 0;

  if(hard) {
    // clear all memory of the emulated machine
    memset(&Pico.ram,0,(unsigned int)&Pico.rom-(unsigned int)&Pico.ram);
  }

  // default VDP register values (based on Fusion)
  Pico.video.reg[0] = Pico.video.reg[1] = 0x04;
  Pico.video.reg[0xc] = 0x81;
  Pico.video.reg[0xf] = 0x02;
  Pico.m.dirtyPal = 1;

  if(PicoRegionOverride)
  {
    support = PicoRegionOverride;
  }
  else
  {
    // Read cartridge region data:
    region=PicoRead32(0x1f0);

    for (i=0;i<4;i++)
    {
      int c=0;

      c=region>>(i<<3); c&=0xff;
      if (c<=' ') continue;

           if (c=='J')  support|=1;
      else if (c=='U')  support|=4;
      else if (c=='E')  support|=8;
      else if (c=='j') {support|=1; break; }
      else if (c=='u') {support|=4; break; }
      else if (c=='e') {support|=8; break; }
      else
      {
        // New style code:
        char s[2]={0,0};
        s[0]=(char)c;
        support|=strtol(s,NULL,16);
      }
    }
  }

  // auto detection order override
  if (PicoAutoRgnOrder) {
         if (((PicoAutoRgnOrder>>0)&0xf) & support) support = (PicoAutoRgnOrder>>0)&0xf;
    else if (((PicoAutoRgnOrder>>4)&0xf) & support) support = (PicoAutoRgnOrder>>4)&0xf;
    else if (((PicoAutoRgnOrder>>8)&0xf) & support) support = (PicoAutoRgnOrder>>8)&0xf;
  }

  // Try to pick the best hardware value for English/50hz:
       if (support&8) { hw=0xc0; pal=1; } // Europe
  else if (support&4)   hw=0x80;          // USA
  else if (support&2) { hw=0x40; pal=1; } // Japan PAL
  else if (support&1)   hw=0x00;          // Japan NTSC
  else hw=0x80; // USA

  Pico.m.hardware=(unsigned char)(hw|0x20); // No disk attached
  Pico.m.pal=pal;
  Pico.video.status = 0x3408 | pal; // always set bits | vblank | pal

  picoSoundReset(); // pal must be known here

  if (PicoMCD & 1) {
    PicoResetMCD(hard);
    return 0;
  }

  // reset sram state; enable sram access by default if it doesn't overlap with ROM
  Pico.m.sram_reg=sram_reg&0x14;
  if (!(Pico.m.sram_reg&4) && Pico.romsize <= SRam.start) Pico.m.sram_reg |= 1;

  elprintf(EL_STATUS, "sram: det: %i; eeprom: %i; start: %06x; end: %06x",
    (Pico.m.sram_reg>>4)&1, (Pico.m.sram_reg>>2)&1, SRam.start, SRam.end);

  return 0;
}


// dma2vram settings are just hacks to unglitch Legend of Galahad (needs <= 104 to work)
// same for Outrunners (92-121, when active is set to 24)
static const int dma_timings[] = {
83,  167, 166,  83, // vblank: 32cell: dma2vram dma2[vs|c]ram vram_fill vram_copy
102, 205, 204, 102, // vblank: 40cell:
16,   16,  15,   8, // active: 32cell:
24,   18,  17,   9  // ...
};

static const int dma_bsycles[] = {
(488<<8)/82,  (488<<8)/167, (488<<8)/166, (488<<8)/83,
(488<<8)/102, (488<<8)/205, (488<<8)/204, (488<<8)/102,
(488<<8)/16,  (488<<8)/16,  (488<<8)/15,  (488<<8)/8,
(488<<8)/24,  (488<<8)/18,  (488<<8)/17,  (488<<8)/9
};

int CheckDMA(void)
{
  int burn = 0, xfers_can, dma_op = Pico.video.reg[0x17]>>6; // see gens for 00 and 01 modes
  int xfers = Pico.m.dma_xfers;
  int dma_op1;

  if(!(dma_op&2)) dma_op = (Pico.video.type==1) ? 0 : 1; // setting dma_timings offset here according to Gens
  dma_op1 = dma_op;
  if(Pico.video.reg[12] & 1) dma_op |= 4; // 40 cell mode?
  if(!(Pico.video.status&8)&&(Pico.video.reg[1]&0x40)) dma_op|=8; // active display?
  xfers_can = dma_timings[dma_op];
  if(xfers <= xfers_can) {
    if(dma_op&2) Pico.video.status&=~2; // dma no longer busy
    else {
      burn = xfers * dma_bsycles[dma_op] >> 8; // have to be approximate because can't afford division..
    }
    Pico.m.dma_xfers = 0;
  } else {
    if(!(dma_op&2)) burn = 488;
    Pico.m.dma_xfers -= xfers_can;
  }

  elprintf(EL_VDPDMA, "~Dma %i op=%i can=%i burn=%i [%i]", Pico.m.dma_xfers, dma_op1, xfers_can, burn, SekCyclesDone());
  //dprintf("~aim: %i, cnt: %i", SekCycleAim, SekCycleCnt);
  return burn;
}

static __inline void SekRunM68k(int cyc)
{
  int cyc_do;
  SekCycleAim+=cyc;
  if((cyc_do=SekCycleAim-SekCycleCnt) <= 0) return;
  PicoCpuCM68k.cycles=cyc_do;
  CycloneRun(&PicoCpuCM68k);
  SekCycleCnt+=cyc_do-PicoCpuCM68k.cycles;
}

static __inline void SekStep(void)
{
  // this is required for timing sensitive stuff to work
  int realaim=SekCycleAim; SekCycleAim=SekCycleCnt+1;
  PicoCpuCM68k.cycles=1;
  CycloneRun(&PicoCpuCM68k);
  SekCycleCnt+=1-PicoCpuCM68k.cycles;
  SekCycleAim=realaim;
}

static int CheckIdle(void)
{
  int i, state[0x22];

  // See if the state is the same after 2 steps:
  SekState(state); SekStep(); SekStep(); SekState(state+0x11);
  for (i = 0x10; i >= 0; i--)
    if (state[i] != state[i+0x11]) return 0;

  return 1;
}


// to be called on 224 or line_sample scanlines only
static __inline void getSamples(int y)
{
  static int curr_pos = 0;

  if(y == 224) {
    if(emustatus & 2)
         curr_pos += picoSoundRender(curr_pos, picoSoundLen-picoSoundLen/2);
    else curr_pos  = picoSoundRender(0, picoSoundLen);
    if (emustatus&1) emustatus|=2; else emustatus&=~2;
  }
  else if(emustatus & 3) {
    emustatus|= 2;
    emustatus&=~1;
    curr_pos = picoSoundRender(0, picoSoundLen/2);
  }
}


#include "pico_frame_hints.h"

extern const unsigned short vcounts[];

// helper z80 runner. Runs only if z80 is enabled at this point
// (z80WriteBusReq will handle the rest)
static void PicoRunZ80Simple(int line_from, int line_to)
{
  int line_from_r=line_from, line_to_r=line_to, line=0;
  int line_sample = Pico.m.pal ? 68 : 93;

  if (!(PicoOpt&4) || Pico.m.z80Run == 0) line_to_r = 0;
  else {
    if (z80startCycle) {
      line = vcounts[z80startCycle>>8];
      if (line > line_from)
        line_from_r = line;
    }
    z80startCycle = SekCyclesDone();
  }

  if (PicoOpt&1) {
    // we have ym2612 enabled, so we have to run Z80 in lines, so we could update DAC and timers
    for (line = line_from; line < line_to; line++) {
      picoSoundTimersAndDac(line);
      if ((line == 224 || line == line_sample) && picoSoundEnabled) getSamples(line);
      if (line == 32 && picoSoundEnabled) emustatus &= ~1;
      if (line >= line_from_r && line < line_to_r)
        z80_run_nr(228);
    }
  } else if (line_to_r-line_from_r > 0) {
    z80_run_nr(228*(line_to_r-line_from_r));
    // samples will be taken by caller
  }
}

// Simple frame without H-Ints
static int PicoFrameSimple(void)
{
  struct PicoVideo *pv=&Pico.video;
  int y=0,line=0,lines=0,lines_step=0,sects;
  int cycles_68k_vblock,cycles_68k_block;

  // split to 16 run calls for active scan, for vblank split to 2 (ntsc), 3 (pal 240), 4 (pal 224)
  if (Pico.m.pal && (pv->reg[1]&8)) {
    if(pv->reg[1]&8) { // 240 lines
      cycles_68k_block  = 7329;  // (488*240+148)/16.0, -4
      cycles_68k_vblock = 11640; // (72*488-148-68)/3.0, 0
      lines_step = 15;
    } else {
      cycles_68k_block  = 6841;  // (488*224+148)/16.0, -4
      cycles_68k_vblock = 10682; // (88*488-148-68)/4.0, 0
      lines_step = 14;
    }
  } else {
    // M68k cycles/frame: 127840.71
    cycles_68k_block  = 6841; // (488*224+148)/16.0, -4
    cycles_68k_vblock = 9164; // (38*488-148-68)/2.0, 0
    lines_step = 14;
  }

  // we don't emulate DMA timing in this mode
  if (Pico.m.dma_xfers) {
    Pico.m.dma_xfers=0;
    Pico.video.status&=~2;
  }

  // VDP FIFO too
  pv->lwrite_cnt = 0;
  Pico.video.status|=0x200;

  Pico.m.scanline=-1;
  z80startCycle=0;

  SekCyclesReset();

  // 6 button pad: let's just say it timed out now
  Pico.m.padTHPhase[0]=Pico.m.padTHPhase[1]=0;

  // ---- Active Scan ----
  pv->status&=~0x88; // clear V-Int, come out of vblank

  // Run in sections:
  for(sects=16; sects; sects--)
  {
    if (CheckIdle()) break;

    lines += lines_step;
    SekRunM68k(cycles_68k_block);

    PicoRunZ80Simple(line, lines);
    line=lines;
  }

  // run Z80 for remaining sections
  if(sects) {
    int c = sects*cycles_68k_block;

    // this "run" is for approriate line counter, etc
    SekCycleCnt += c;
    SekCycleAim += c;

    lines += sects*lines_step;
    PicoRunZ80Simple(line, lines);
  }

  // render screen
  if (!PicoSkipFrame)
  {
	if (!(PicoOpt&0x10)) {
      // Draw the screen
      if (pv->reg[1]&8) {
        for (y=0;y<240;y++) PicoLine(y);
      } else {
        for (y=0;y<224;y++) PicoLine(y);
      }
	} else {
		PicoFrameFull();
	}
  }

  // here we render sound if ym2612 is disabled
  if (!(PicoOpt&1) && picoSoundEnabled) {
	picoSoundRender(0, picoSoundLen);
  }

  // a gap between flags set and vint
  pv->pending_ints|=0x20;
  pv->status|=8; // go into vblank
  SekRunM68k(68+4);

  // ---- V-Blanking period ----
  // fix line counts
  if(Pico.m.pal) {
    if(pv->reg[1]&8) { // 240 lines
      lines = line = 240;
      sects = 3;
      lines_step = 24;
    } else {
      lines = line = 224;
      sects = 4;
      lines_step = 22;
    }
  } else {
    lines = line = 224;
    sects = 2;
    lines_step = 19;
  }

  if (pv->reg[1]&0x20) SekInterrupt(6); // Set IRQ
  if (Pico.m.z80Run && (PicoOpt&4))
    z80_int();

  while (sects) {
    lines += lines_step;

    SekRunM68k(cycles_68k_vblock);

    PicoRunZ80Simple(line, lines);
    line=lines;

    sects--;
    if (sects && CheckIdle()) break;
  }

  // run Z80 for remaining sections
  if (sects) {
    lines += sects*lines_step;
    PicoRunZ80Simple(line, lines);
  }

  return 0;
}

int PicoFrame(void)
{
  int acc;

  if (PicoMCD & 1) {
    PicoFrameMCD();
    return 0;
  }

  // be accurate if we are asked for this
  if(PicoOpt&0x40) acc=1;
  // don't be accurate in alternative render mode, as hint effects will not be rendered anyway
  else if(PicoOpt&0x10) acc = 0;
  else acc=Pico.video.reg[0]&0x10; // be accurate if hints are used

  //if(Pico.video.reg[12]&0x2) Pico.video.status ^= 0x10; // change odd bit in interlace mode

  if(!(PicoOpt&0x10))
    PicoFrameStart();

  if(acc)
       PicoFrameHints();
  else PicoFrameSimple();

  return 0;
}

void PicoFrameDrawOnly(void)
{
  int y;
  PicoFrameStart();
  for (y=0;y<224;y++) PicoLine(y);
}

PicoEmu picoEmu;
QImage picoFrame;

PicoEmu::PicoEmu() :
	Emu("pico"),
	m_lastVideoMode(0),
	m_mp3Player(0)
{
}

static void *md_screen = 0;
unsigned char *PicoDraw2FB = 0;  // temporary buffer for alt renderer

void picoScanLine(uint num) {
	DrawLineDest = (u16 *)md_screen + 320*(num+1);
}

bool PicoEmu::findMcdBios(QString *biosFileName, QString *error)
{
	QStringList possibleBiosNames;
	int region = Pico_mcd->TOC.region();

	if (region == PicoRegionUsa) {
		possibleBiosNames << "us_scd2_9306.bin";
		possibleBiosNames << "us_scd2_9303.bin";
		possibleBiosNames << "us_scd1_9210.bin";
	} else if (region == PicoRegionEurope) {
		possibleBiosNames << "eu_mcd2_9306.bin";
		possibleBiosNames << "eu_mcd2_9303.bin";
		possibleBiosNames << "eu_mcd1_9210.bin";
	} else if (region == PicoRegionJapanNtsc || region == PicoRegionJapanPal) {
		possibleBiosNames << "jp_mcd1_9112.bin";
		possibleBiosNames << "jp_mcd1_9111.bin";
	} else {
		Q_ASSERT(false);
		*error = QObject::tr("Internal error");
	}

	*biosFileName = QString();
	QDir dir(pathManager.diskDirPath());
	foreach (QString name, possibleBiosNames) {
		if (dir.exists(name)) {
			*biosFileName = dir.filePath(name);
			break;
		}
	}
	if (biosFileName->isEmpty()) {
		*error = QObject::tr("BIOS file not found");
		return false;
	}

	// the bios must be of size 128KB
	QFileInfo biosFileInfo(*biosFileName);
	if (biosFileInfo.size() != 128 * 1024) {
		*error = QObject::tr("Invalid BIOS size: %1 should be %2 bytes large")
				.arg(biosFileInfo.size())
				.arg(128 * 1024);
		return false;
	}

	return true;
}

bool PicoEmu::init(const QString &diskPath, QString *error)
{
	QString cartPath = diskPath;

	PicoOpt = 0x0f | 0x20 | 0xe00 | 0x1000; // | use_940, cd_pcm, cd_cdda, scale/rot
	PicoAutoRgnOrder = 0x184; // US, EU, JP

	// make temp buffer for alt renderer
	PicoDraw2FB = (unsigned char *)malloc((8+320)*(8+240+8));
	PicoInit();

	picoFrame = QImage(320, 240, QImage::Format_RGB16);
	md_screen = picoFrame.bits();

	// make sure we are in correct mode
	picoScanLine(0);
	Pico.m.dirtyPal = 1;
	PicoOpt &= ~0x4100;
	PicoOpt |= 0x0100;
	m_lastVideoMode = ((Pico.video.reg[12]&1)<<2) ^ 0xc;

	if (!diskPath.endsWith(".gen") && !diskPath.endsWith(".smd")) {
		QString fileName = diskPath;
		QFileInfo fileInfo(diskPath);
		if (fileInfo.isDir()) {
			QString name = fileInfo.fileName();
			fileName = QString("%1/%2.iso").arg(diskPath).arg(name);
			if (!QFile::exists(fileName)) {
				fileName = QString("%1/%2.bin").arg(diskPath).arg(name);
				if (!QFile::exists(fileName)) {
					*error = tr("iso/bin not found in the directory");
					return false;
				}
			}
		}

		mcd_state *data = new mcd_state;
		memset(data, 0, sizeof(mcd_state));
		Pico.rom = picoRom = (u8 *)data;
		// CD
		if (!Insert_CD(fileName, error))
			return false;
		PicoMCD |= 1;
		picoMcdOpt |= PicoMcdEnabled;

		if (!findMcdBios(&cartPath, error))
			return false;

		if (Pico_mcd->TOC.Last_Track > 1)
			m_mp3Player = new Mp3Player(this);
	}

	if (!picoCart.open(cartPath, error))
		return false;

	PicoReset(1);
	PicoOpt &= ~0x10000;
	// pal/ntsc might have changed, reset related stuff
	int targetFps = Pico.m.pal ? 50 : 60;
	setFrameRate(targetFps);
	return true;
}

void PicoEmu::shutdown()
{
	picoFrame = QImage();
}

void PicoEmu::reset()
{
	PicoReset(0);
}

void PicoEmu::emulateFrame(bool drawEnabled)
{
	updateInput();

	md_screen = picoFrame.bits();
	int oldmodes = m_lastVideoMode;

	// check for mode changes
	m_lastVideoMode = ((Pico.video.reg[12]&1)<<2)|(Pico.video.reg[1]&8);
	if (m_lastVideoMode != oldmodes) {
		int w = ((m_lastVideoMode & 4) ? 320 : 256);
		int h = ((m_lastVideoMode & 8) ? 240 : 224);
		setVideoSrcRect(QRectF(0, 0, w, h));
	}
	PicoSkipFrame = (drawEnabled ? 0 : 2);
	PicoFrame();
}

const QImage &PicoEmu::frame() const
{
	return picoFrame;
}

void PicoEmu::pause()
{
	if (mp3Player()) {
		QMetaObject::invokeMethod(mp3Player(),
								  "pause",
								  Qt::QueuedConnection);
	}
}

void PicoEmu::resume()
{
	picoSoundEnabled = isAudioEnabled();

	if (mp3Player()) {
		QMetaObject::invokeMethod(mp3Player(),
								  "resume",
								  Qt::QueuedConnection);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		return -1;
	QApplication app(argc, argv);
	EmuView view(&picoEmu, argv[1]);
	return app.exec();
}
