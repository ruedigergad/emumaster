/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

#include "gpu_unai.h"
#include "psx.h"
#include <QImage>
#include <QDataStream>

#define	FRAME_BUFFER_SIZE	(1024*512*2)
#define	FRAME_WIDTH			  1024
#define	FRAME_HEIGHT		  512
#define	FRAME_OFFSET(x,y)	(((y)<<10)+(x))

#include "gpu_unai_fixedpoint.h"

struct GPUPacket {
	union
	{
		u32 U4[16];
		s32 S4[16];
		u16 U2[32];
		s16 S2[32];
		u8  U1[64];
		s8  S1[64];
	};
};

PsxGpuUnai psxGpuUnai;

static bool isSkip = false; /* skip frame (info coming from GPU) */

static bool light = true; /* lighting */
static bool blend = true; /* blending */
static bool FrameToRead = false; /* load image in progress */
static bool FrameToWrite = false; /* store image in progress */

static bool enableAbbeyHack = false; /* Abe's Odyssey hack */

static u8 BLEND_MODE;
static u8 TEXT_MODE;
static u8 Masking;

static u16 PixelMSB;
static u16 PixelData;

///////////////////////////////////////////////////////////////////////////////
//  GPU Global data
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Dma Transfers info
static s32 px, py;
static s32 x_end, y_end;
static u16 *pvram;

static u32 GP0;
static s32 PacketCount;
static s32 PacketIndex;

///////////////////////////////////////////////////////////////////////////////
//  Display status
static u32 DisplayArea   [6];

///////////////////////////////////////////////////////////////////////////////
//  Rasterizer status
static u32 TextureWindow [4];
static u32 DrawingArea   [4];
static u32 DrawingOffset [2];

///////////////////////////////////////////////////////////////////////////////
//  Rasterizer status

static u16* TBA;
static u16* CBA;

///////////////////////////////////////////////////////////////////////////////
//  Inner Loops
static s32 u4, du4;
static s32 v4, dv4;
static s32 r4, dr4;
static s32 g4, dg4;
static s32 b4, db4;
static u32 lInc;
static u32 tInc, tMsk;

static GPUPacket PacketBuffer;
// FRAME_BUFFER_SIZE is defined in bytes; 512K is guard memory for out of range reads
static u16 GPU_FrameBuffer[(FRAME_BUFFER_SIZE+512*1024)/2] __attribute__((aligned(16)));
static u32 GPU_GP1;

///////////////////////////////////////////////////////////////////////////////
//  Inner loop driver instanciation file
#include "gpu_unai_inner.h"

///////////////////////////////////////////////////////////////////////////////
//  GPU Raster Macros
#define	GPU_RGB16(rgb)        ((((rgb)&0xF80000)>>9)|(((rgb)&0xF800)>>6)|(((rgb)&0xF8)>>3))

#define GPU_EXPANDSIGN_POLY(x)  (((s32)(x)<<20)>>20)
//#define GPU_EXPANDSIGN_POLY(x)  (((s32)(x)<<21)>>21)
#define GPU_EXPANDSIGN_SPRT(x)  (((s32)(x)<<21)>>21)

//#define	GPU_TESTRANGE(x)      { if((u32)(x+1024) > 2047) return; }
#define	GPU_TESTRANGE(x)      { if ((x<-1023) || (x>1023)) return; }

#define	GPU_SWAP(a,b,t)	{(t)=(a);(a)=(b);(b)=(t);}

///////////////////////////////////////////////////////////////////////////////
// GPU internal image drawing functions
#include "gpu_unai_raster_image.h"

///////////////////////////////////////////////////////////////////////////////
// GPU internal line drawing functions
#include "gpu_unai_raster_line.h"

///////////////////////////////////////////////////////////////////////////////
// GPU internal polygon drawing functions
#include "gpu_unai_raster_polygon.h"

///////////////////////////////////////////////////////////////////////////////
// GPU internal sprite drawing functions
#include "gpu_unai_raster_sprite.h"

///////////////////////////////////////////////////////////////////////////////
// GPU command buffer execution/store
#include "gpu_unai_command.h"

///////////////////////////////////////////////////////////////////////////////
static inline void gpuReset()
{
	GPU_GP1 = 0x14802000;
	TextureWindow[0] = 0;
	TextureWindow[1] = 0;
	TextureWindow[2] = 255;
	TextureWindow[3] = 255;
	DrawingArea[2] = 256;
	DrawingArea[3] = 240;
	DisplayArea[2] = 256;
	DisplayArea[3] = 240;
	DisplayArea[5] = 240;
}

///////////////////////////////////////////////////////////////////////////////
//  GPU DMA comunication

///////////////////////////////////////////////////////////////////////////////
static const u8 PacketSize[256] = {
	0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//		0-15
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//		16-31
	3, 3, 3, 3, 6, 6, 6, 6, 4, 4, 4, 4, 8, 8, 8, 8,	//		32-47
	5, 5, 5, 5, 8, 8, 8, 8, 7, 7, 7, 7, 11, 11, 11, 11,	//	48-63
	2, 2, 2, 2, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3,	//		64-79
	3, 3, 3, 3, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4,	//		80-95
	2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 1, 1, 2, 2, 2, 2,	//		96-111
	1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2,	//		112-127
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//		128-
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//		144
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//		160
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	//
};

///////////////////////////////////////////////////////////////////////////////
static inline void gpuSendPacket()
{
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_sendPacket++;
#endif
	gpuSendPacketFunction(PacketBuffer.U4[0]>>24);
}

///////////////////////////////////////////////////////////////////////////////
static inline void gpuCheckPacket(u32 uData)
{
	if (PacketCount)
	{
		PacketBuffer.U4[PacketIndex++] = uData;
		--PacketCount;
	}
	else
	{
		PacketBuffer.U4[0] = uData;
		PacketCount = PacketSize[uData >> 24];
		PacketIndex = 1;
	}
	if (!PacketCount) gpuSendPacket();
}

///////////////////////////////////////////////////////////////////////////////
static void gpuUnaiWriteDataMem(u32* dmaAddress, s32 dmaCount)
{
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_writeDataMem++;
#endif
	u32 data;
	const u16 *VIDEO_END=(GPU_FrameBuffer+(FRAME_BUFFER_SIZE/2)-1);
	GPU_GP1 &= ~0x14000000;

	while (dmaCount) 
	{
		if (FrameToWrite) 
		{
			while (dmaCount--) 
			{
				data = *dmaAddress++;
				if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
				pvram[px] = data;
				if (++px>=x_end) 
				{
					px = 0;
					pvram += 1024;
					if (++py>=y_end) 
					{
						FrameToWrite = false;
						GPU_GP1 &= ~0x08000000;
						break;
					}
				}
				if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
				pvram[px] = data>>16;
				if (++px>=x_end) 
				{
					px = 0;
					pvram += 1024;
					if (++py>=y_end) 
					{
						FrameToWrite = false;
						GPU_GP1 &= ~0x08000000;
						break;
					}
				}
			}
		}
		else
		{
			data = *dmaAddress++;
			dmaCount--;
			gpuCheckPacket(data);
		}
	}

	GPU_GP1 = (GPU_GP1 | 0x14000000) & ~0x60000000;
}

u32 *lUsedAddr[3];
static inline int CheckForEndlessLoop(u32 *laddr)
{
	if(laddr==lUsedAddr[1]) return 1;
	if(laddr==lUsedAddr[2]) return 1;

	if(laddr<lUsedAddr[0]) lUsedAddr[1]=laddr;
	else                   lUsedAddr[2]=laddr;
	lUsedAddr[0]=laddr;
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static u32 gpuUnaiDmaChain(u32* baseAddr, u32 dmaVAddr)
{
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_dmaChain++;
#endif
	u32 data, *address, count, offset;
	unsigned int DMACommandCounter = 0;
	long dma_words = 0;

	GPU_GP1 &= ~0x14000000;
	lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=(u32*)0x1fffff;
	dmaVAddr &= 0x001FFFFF;
	while (dmaVAddr != 0x1FFFFF)
	{
		address = (baseAddr + (dmaVAddr >> 2));
		if(DMACommandCounter++ > 2000000) break;
		if(CheckForEndlessLoop(address)) break;
		data = *address++;
		count = (data >> 24);
		offset = data & 0x001FFFFF;
		if (dmaVAddr != offset) dmaVAddr = offset;
		else dmaVAddr = 0x1FFFFF;

		if(count>0) gpuUnaiWriteDataMem(address,count);
		dma_words += 1 + count;
	}
	GPU_GP1 = (GPU_GP1 | 0x14000000) & ~0x60000000;

	return dma_words;
}

///////////////////////////////////////////////////////////////////////////////
static void gpuUnaiWriteData(u32 data)
{
	const u16 *VIDEO_END=(GPU_FrameBuffer+(FRAME_BUFFER_SIZE/2)-1);
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_writeData++;
#endif
	GPU_GP1 &= ~0x14000000;

	if (FrameToWrite)
	{
		if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
		pvram[px]=(u16)data;
		if (++px>=x_end)
		{
			px = 0;
			pvram += 1024;
			if (++py>=y_end) 
			{
				FrameToWrite = false;
				GPU_GP1 &= ~0x08000000;
			}
		}
		if (FrameToWrite)
		{
			if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
			pvram[px]=data>>16;
			if (++px>=x_end)
			{
				px = 0;
				pvram += 1024;
				if (++py>=y_end) 
				{
					FrameToWrite = false;
					GPU_GP1 &= ~0x08000000;
				}
			}
		}
	}
	else
	{
		gpuCheckPacket(data);
	}
	GPU_GP1 |= 0x14000000;
}


///////////////////////////////////////////////////////////////////////////////
static void gpuUnaiReadDataMem(u32* dmaAddress, s32 dmaCount)
{
	const u16 *VIDEO_END=(GPU_FrameBuffer+(FRAME_BUFFER_SIZE/2)-1);
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_readDataMem++;
#endif
	if(!FrameToRead) return;

	GPU_GP1 &= ~0x14000000;
	do 
	{
		if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
		// lower 16 bit
		u32 data = (unsigned long)pvram[px];

		if (++px>=x_end) 
		{
			px = 0;
			pvram += 1024;
		}

		if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
		// higher 16 bit (always, even if it's an odd width)
		data |= (unsigned long)(pvram[px])<<16;
		
		*dmaAddress++ = data;

		if (++px>=x_end) 
		{
			px = 0;
			pvram += 1024;
			if (++py>=y_end) 
			{
				FrameToRead = false;
				GPU_GP1 &= ~0x08000000;
				break;
			}
		}
	} while (--dmaCount);

	GPU_GP1 = (GPU_GP1 | 0x14000000) & ~0x60000000;
}



///////////////////////////////////////////////////////////////////////////////
static u32 gpuUnaiReadData(void)
{
	const u16 *VIDEO_END=(GPU_FrameBuffer+(FRAME_BUFFER_SIZE/2)-1);
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_readData++;
#endif
	GPU_GP1 &= ~0x14000000;
	if (FrameToRead)
	{
		if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
		GP0 = pvram[px];
		if (++px>=x_end)
		{
			px = 0;
			pvram += 1024;
			if (++py>=y_end) 
			{
				FrameToRead = false;
				GPU_GP1 &= ~0x08000000;
			}
		}
		if ((&pvram[px])>(VIDEO_END)) pvram-=512*1024;
		GP0 |= pvram[px]<<16;
		if (++px>=x_end)
		{
			px = 0;
			pvram +=1024;
			if (++py>=y_end) 
			{
				FrameToRead = false;
				GPU_GP1 &= ~0x08000000;
			}
		}

	}
	GPU_GP1 |= 0x14000000;
	return (GP0);
}

///////////////////////////////////////////////////////////////////////////////
static u32 gpuUnaiReadStatus(void)
{
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_readStatus++;
#endif
	return GPU_GP1;
}

///////////////////////////////////////////////////////////////////////////////
static void gpuUnaiWriteStatus(u32 data)
{
#ifdef DEBUG_ANALYSIS
	dbg_anacnt_GPU_writeStatus++;
#endif
	switch (data >> 24) {
	case 0x00:
		gpuReset();
		break;
	case 0x01:
		GPU_GP1 &= ~0x08000000;
		PacketCount = 0; FrameToRead = FrameToWrite = false;
		break;
	case 0x02:
		GPU_GP1 &= ~0x08000000;
		PacketCount = 0; FrameToRead = FrameToWrite = false;
		break;
	case 0x03:
		GPU_GP1 = (GPU_GP1 & ~0x00800000) | ((data & 1) << 23);
		break;
	case 0x04:
		if (data == 0x04000000)
		PacketCount = 0;
		GPU_GP1 = (GPU_GP1 & ~0x60000000) | ((data & 3) << 29);
		break;
	case 0x05: {
		int oldX = DisplayArea[0];
		int oldY = DisplayArea[1];
		DisplayArea[0] = (data & 0x000003FF);
		DisplayArea[1] = ((data & 0x0007FC00)>>10);
		isSkip = (isSkip && oldX == DisplayArea[0] && oldY == DisplayArea[1]);
		break;
	}
	case 0x07:
		DisplayArea[4] = data & 0x000003FF; //(short)(data & 0x3ff);
		DisplayArea[5] = (data & 0x000FFC00) >> 10; //(short)((data>>10) & 0x3ff);
		break;
	case 0x08:
		{
			GPU_GP1 = (GPU_GP1 & ~0x007F0000) | ((data & 0x3F) << 17) | ((data & 0x40) << 10);
			static u32 HorizontalResolution[8] = { 256, 368, 320, 384, 512, 512, 640, 640 };
			DisplayArea[2] = HorizontalResolution[(GPU_GP1 >> 16) & 7];
			static u32 VerticalResolution[4] = { 240, 480, 256, 480 };
			DisplayArea[3] = VerticalResolution[(GPU_GP1 >> 19) & 3];
//			isPAL = (data & 0x08) ? true : false; // if 1 - PAL mode, else NTSC
		}
		break;
	case 0x10:
		switch (data & 0xffff) {
		case 0:
		case 1:
		case 3:
			GP0 = (DrawingArea[1] << 10) | DrawingArea[0];
			break;
		case 4:
			GP0 = ((DrawingArea[3]-1) << 10) | (DrawingArea[2]-1);
			break;
		case 6:
		case 5:
			GP0 = (DrawingOffset[1] << 11) | DrawingOffset[0];
			break;
		case 7:
			GP0 = 2;
			break;
		default:
			GP0 = 0;
		}
		break;
	}
}

extern "C" void gpuUnaiConvertBgr555ToRgb565(void *dst, void *src, int width, int height);

static QImage gpuFrame;

static void gpuVideoOutput() {
	int x0 = DisplayArea[0];
	int y0 = DisplayArea[1];

	int w0 = DisplayArea[2];
	int h0 = DisplayArea[3];  // video mode

	int h1 = DisplayArea[5] - DisplayArea[4]; // display needed
	if (h0 == 480)
		h1 = Min2(h1*2,480);

	if (h1 <= 0)
		return;

	u32* src  = (u32*)(&((u16*)GPU_FrameBuffer)[FRAME_OFFSET(x0,y0)]);
	int width = (w0+63)/64*64;
	QImage::Format fmt = (GPU_GP1 & 0x00200000 ? QImage::Format_RGB888 : QImage::Format_RGB16);
	if (width != gpuFrame.width() || h1 != gpuFrame.height() || fmt != gpuFrame.format()) {
		if (fmt == QImage::Format_RGB16) {
			gpuFrame = QImage(width, h1, QImage::Format_RGB16);
			qDebug("16 bit video %d %d", w0, h1);
		} else {
			qDebug("24 bit video %d %d", w0, h1);
			gpuFrame = QImage((uchar *)src, width, h1, FRAME_WIDTH*2, QImage::Format_RGB888);
		}
		psxEmu.updateGpuScale(w0, h1);
	} else {
		if (fmt == QImage::Format_RGB16) {
			gpuUnaiConvertBgr555ToRgb565(gpuFrame.bits(), src, (w0+63)/64*64, h1);
		} else {
			gpuFrame = QImage((uchar *)src, width, h1, FRAME_WIDTH*2, QImage::Format_RGB888);
		}
	}
}

static void gpuUnaiUpdateLace() {
	// Interlace bit toggle
	GPU_GP1 ^= 0x80000000;
	if (GPU_GP1 & 0x08800000) // Display disabled
		return;
	if (!isSkip)
		gpuVideoOutput();
	psxEmu.flipScreen();
}

bool PsxGpuUnai::init() {
	GPU_writeStatus		= gpuUnaiWriteStatus;
	GPU_writeData		= gpuUnaiWriteData;
	GPU_writeDataMem	= gpuUnaiWriteDataMem;
	GPU_readStatus		= gpuUnaiReadStatus;
	GPU_readData		= gpuUnaiReadData;
	GPU_readDataMem		= gpuUnaiReadDataMem;
	GPU_dmaChain		= gpuUnaiDmaChain;
	GPU_updateLace		= gpuUnaiUpdateLace;

	gpuReset();

	// s_invTable
	for(int i=1;i<=(1<<TABLE_BITS);++i)
	{
		double v = 1.0 / double(i);
		#ifdef GPU_TABLE_10_BITS
		v *= double(0xffffffff>>1);
		#else
		v *= double(0x80000000);
		#endif
		s_invTable[i-1]=s32(v);
	}
	return true;
}

void PsxGpuUnai::shutdown() {
	gpuFrame = QImage();
}

const QImage & PsxGpuUnai::frame()
{ return gpuFrame; }

void PsxGpuUnai::setDrawEnabled(bool drawEnabled)
{ isSkip = !drawEnabled; }

void PsxGpuUnai::sl() {
	emsl.begin("gpu");
	emsl.var("gp1", GPU_GP1);
	emsl.array("frameBuffer", GPU_FrameBuffer, sizeof(GPU_FrameBuffer));
	emsl.array("displayArea", DisplayArea, sizeof(DisplayArea));
	emsl.array("textureWindow", TextureWindow, sizeof(TextureWindow));
	emsl.array("drawingArea", DrawingArea, sizeof(DrawingArea));
	emsl.array("drawingOffset", DrawingOffset, sizeof(DrawingOffset));
	emsl.end();
}
