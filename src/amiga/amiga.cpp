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

#include "amiga.h"
#include "events.h"
#include "mem.h"
#include "spu.h"
#include "mem.h"
#include "custom.h"
#include "cpu.h"
#include "disk.h"
#include "drawing.h"
#include "cia.h"
#include <base/emuview.h>
#include <base/pathmanager.h>
#include <QImage>
#include <QSemaphore>
#include <QApplication>

static volatile bool amigaGoingShutdown = false;
static QImage amigaFrame;

AmigaEmu amigaEmu;
AmigaThread amigaThread;

QSemaphore frameConsSem;
QSemaphore frameProdSem;

AmigaEmu::AmigaEmu(QObject *parent) :
	Emu("amiga", parent) {
}

void AmigaEmu::reset() {
	amigaCpuSetSpcFlag(SpcFlagBrk);
}

bool AmigaEmu::init(const QString &diskPath, QString *error) {
	setFrameRate(50);
	// TODO ntsc/pal configurable
	setVideoSrcRect(QRect(0, MINFIRSTLINE_PAL, 320, MAXVPOS_PAL-MINFIRSTLINE_PAL-VBLANK_ENDLINE_PAL));
	amigaFrame = QImage(320, MAXVPOS, QImage::Format_RGB16);

	amigaMemChipSize = 0x00100000; // TODO configurable chip size
	amigaMemInit();

	if (!amigaLoadKickstart(pathManager.diskDirPath()+"/kick13.rom")) {
		*error = tr("Could not load kickstart");
		return false;
	}

	if (!amigaDrives[0].insertDisk(diskPath)) {
		*error = EM_MSG_DISK_LOAD_FAILED;
		return false;
	}

	amigaThread.start();
	frameConsSem.acquire();
	return true;
}

void AmigaEmu::shutdown() {
	amigaCpuSetSpcFlag(SpcFlagBrk);
	amigaCpuReleaseTimeslice();
	amigaGoingShutdown = true;
	frameProdSem.release();
	amigaThread.wait(2000);
	amigaFrame = QImage();
}

void AmigaEmu::emulateFrame(bool drawEnabled) {
	amigaDrawEnabled = drawEnabled;
	amigaFrame.bits();
	frameProdSem.release();
	frameConsSem.acquire();
	updateInput();
}

const QImage &AmigaEmu::frame() const
{ return amigaFrame; }
int AmigaEmu::fillAudioBuffer(char *stream, int streamSize)
{ return amigaSpuFillAudioBuffer(stream, streamSize); }

void AmigaEmu::setJoy(int joy, int buttons) {
	amigaInputPortButtons[joy] |= buttons >> 4;

	if (buttons)
		m_inputPortToggle[joy] = false;
	if (!m_inputPortToggle[joy])
		amigaInputPortDir[joy] = 0;
	if (buttons & 0xF) {
		if (buttons & EmuPad::Button_Up)
			amigaInputPortDir[joy] |= (1 << 8);
		if (buttons & EmuPad::Button_Down)
			amigaInputPortDir[joy] |= (1 << 0);
		if (buttons & EmuPad::Button_Left)
			amigaInputPortDir[joy] ^= (3 << 8);
		if (buttons & EmuPad::Button_Right)
			amigaInputPortDir[joy] ^= (3 << 0);
	}
}

void AmigaEmu::updateInput() {
	amigaInputPortButtons[0] = 0;
	amigaInputPortButtons[1] = 0;

	setJoy(0, input()->pad[0].buttons());
	setJoy(1, input()->pad[1].buttons());

	const EmuMouse &mouse0 = input()->mouse[0];
	setMouse(0, mouse0.buttons(), mouse0.xRel(), mouse0.yRel());
	const EmuMouse &mouse1 = input()->mouse[1];
	setMouse(1, mouse1.buttons(), mouse1.xRel(), mouse1.yRel());

	int key;
	do {
		key = input()->keyb.dequeue();
		if (key)
			amigaRecordKey(key & ~(1<<31), key & (1<<31));
	} while (key != 0);
}

void AmigaEmu::setMouse(int mouse, int buttons, int dx, int dy) {
	dx = qBound(-127, dx/2, 127);
	dy = qBound(-127, dy/2, 127);
	if (!(dx | dy | buttons))
		return;

	m_inputPortToggle[mouse] = true;
	amigaInputPortButtons[mouse] |= buttons;
	u16 oldMouseRel = amigaInputPortDir[mouse];
	u8 mouseX = (oldMouseRel >> 0) & 0xFF;
	u8 mouseY = (oldMouseRel >> 8) & 0xFF;
	mouseX += dx;
	mouseY += dy;
	amigaInputPortDir[mouse] = (mouseY << 8) | mouseX;
}

void AmigaThread::run() {
	amigaCustomInit();
	amigaDiskReset();
	amigaCpuInit();
	amigaDrawInit((char *)amigaFrame.bits(), amigaFrame.bytesPerLine());
	amigaEventsInit();
	amigaCustomReset();
	amigaSpuDefaultEvtime();

	while (!amigaGoingShutdown) {
		amigaCpuReset();
		amigaCpuRun();
	}

	amigaCustomShutdown();
	amigaMemShutdown();
}

void AmigaEmu::sl() {
	amigaCpuSl();
	amigaMemSl();
	amigaDiskSl();
	amigaCustomSl();
	amigaSpuSl();
	amigaCiaSl();
}

int main(int argc, char *argv[]) {
	if (argc < 2)
		return -1;
	QApplication app(argc, argv);
	EmuView view(&amigaEmu, argv[1]);
	return app.exec();
}
