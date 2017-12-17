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

#include "psx.h"
#include "common.h"
#include "cdrom.h"
#include "misc.h"
#include "mdec.h"
#include "sio.h"
#include "ppf.h"
#include "cpu_int.h"
#include "cpu_rec.h"
#include "gpu_unai.h"
#include "spu_null.h"
#include "spu_fran.h"
#include "mem.h"
#include "hw.h"
#include "bios.h"
#include "cdriso.h"
#include "counters.h"

#include <base/pathmanager.h>
#include <base/emuview.h>
#include <QString>
#include <QImage>
#include <QPainter>
#include <QApplication>
#include <QGuiApplication>
#include <QQuickView>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <sys/time.h>

PcsxConfig Config;

static void dummy_lace() { }

PsxEmu psxEmu;
PsxThread psxThread;

PsxEmu::PsxEmu(QObject *parent) :
	Emu("psx", parent) {
}

bool PsxEmu::init(const QString &diskPath, QString *error) {
	Config.HLE = 0;

	psxMcd1.init(pathManager.userDataDirPath() + "/psx_mcd1.mcr");
	psxMcd2.init(pathManager.userDataDirPath() + "/psx_mcd2.mcr");
	Config.PsxAuto = 1;
	Config.Cdda = 0;
	Config.Xa = 0;
	Config.Mdec = 0;
	Config.PsxOut = 0;
	Config.RCntFix = 0;
	Config.Sio = 0;
	Config.SpuIrq = 0;
	Config.VSyncWA = 0;
	// TODO give user ability to choose his bios
	QDir diskDir(pathManager.diskDirPath());
	QStringList biosFilter;
	biosFilter << "scph*.bin";
	QStringList biosList = diskDir.entryList(biosFilter, QDir::NoFilter, QDir::Name);
	if (!biosList.isEmpty())
		psxMem.setBiosName(biosList.at(0));

	systemType = NtscType;
	setVideoSrcRect(QRect(0, 0, 256, 240));
	setFrameRate(60);

	// TODO option to set interpreter cpu
//	if (Config.Cpu == CpuInterpreter)
//		psxCpu = &psxInt;
//	else
		psxCpu = &psxRec;

		psxGpu = &psxGpuUnai;
		psxSpu = &psxSpuFran;

	if (!psxMemInit()) {
		*error = "Could not allocate memory!";
		return false;
	}
	if (!psxCpu->init()) {
		*error = "Could not initialize CPU!";
		return false;
	}

	// TODO debug as preprocessor
	if (Config.Debug) {
		StartDebugger();
	}

	cdrIsoInit();
	if (CDR_init() < 0) {
		*error = tr("Could not initialize CD-ROM!");
		return false;
	}
	if (!psxGpu->init()) {
		*error = tr("Could not initialize GPU!");
		return false;
	}
	if (!psxSpu->init()) {
		*error = tr("Could not initialize SPU!");
		return false;
	}
	*error = setDisk(diskPath);
	return error->isEmpty();
}

void PsxEmu::shutdown() {
	stop = true;
	m_prodSem.release();
	psxThread.wait();
	psxGpu->shutdown();
	psxSpu->shutdown();
	psxCpu->shutdown();
	psxMemShutdown();
	FreePPFCache();
	StopDebugger();
}

void PsxEmu::reset() {
	// rearmed hack: reset runs some code when real BIOS is used,
	// but we usually do reset from menu while GPU is not open yet,
	// so we need to prevent updateLace() call..
	void (*real_lace)() = GPU_updateLace;
	GPU_updateLace = dummy_lace;

	psxMemReset();

	memset(&psxRegs, 0, sizeof(psxRegs));
	psxRegs.pc = 0xBFC00000; // Start in bootstrap
	psxRegs.CP0.r[12] = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.r[15] = 0x00000002; // PRevID = Revision ID, same as R3000A

	psxCpu->reset();

	psxHwReset();
	psxBiosInit();

	if (!Config.HLE) {
		// skip bios logo TODO option
		while (psxRegs.pc != 0x80030000)
			psxCpu->executeBlock();
	}

	// hmh core forgets this
	CDR_stop();

	GPU_updateLace = real_lace;

	CheckCdrom();
	LoadCdrom();
}

void PsxEmu::updateGpuScale(int w, int h) {
	setVideoSrcRect(QRect(0, 0, w, h));
}

void PsxEmu::flipScreen() {
	m_consSem.release();
	m_prodSem.acquire();
}

QString PsxEmu::setDisk(const QString &path) {
	SetCdOpenCaseTime(time(0) + 2);
	SetIsoFile(path.toLocal8Bit().constData());
	if (!psxThread.isRunning()) {
		if (CDR_open() < 0)
			return tr("Could not open CD-ROM.");
		reset();
		if (CheckCdrom() == -1)
			return tr("Could not load CD.");

		setFrameRate(systemType == NtscType ? 60 : 50);
		psxThread.start();
		m_consSem.acquire();
	}
	return QString();
}

void PsxEmu::emulateFrame(bool drawEnabled) {
	psxGpu->setDrawEnabled(drawEnabled);
	setPadKeys(0, input()->pad[0].buttons());
	setPadKeys(1, input()->pad[1].buttons());
	m_prodSem.release();
	m_consSem.acquire();
}

const QImage &PsxEmu::frame() const
{ return psxGpu->frame(); }

int PsxEmu::fillAudioBuffer(char *stream, int streamSize)
{ return psxSpu->fillBuffer(stream, streamSize); }

void PsxEmu::resume()
{
	psxSpu->setEnabled(isAudioEnabled());
}

extern void setPadButtons(int emuKeys);

void PsxEmu::setPadKeys(int pad, int keys) {
	if (pad)
		return;
	// TODO many pads
	setPadButtons(keys);
}

void PsxThread::run() {
	psxCpu->execute();
}

void PsxEmu::sl() {
	// TODO hard config
	emsl.begin("machine");
	emsl.var("hle", Config.HLE);
	emsl.var("systemType", systemType);
	emsl.end();

	if (emsl.save) {
		new_dyna_save();
		if (Config.HLE)
			psxBiosFreeze(1);
	} else {
		if (Config.HLE)
			psxBiosInit();
	}
	emsl.begin("mem");
	emsl.array("psxM", psxM, 0x00200000);
	emsl.array("psxR", psxR, 0x00080000);
	emsl.array("psxH", psxH, 0x00010000);
	emsl.array("psxRegs", &psxRegs, sizeof(psxRegs));
	emsl.end();

	if (!emsl.save) {
		if (Config.HLE)
			psxBiosFreeze(0);
		psxCpu->reset();
	}

	psxSioSl();
	psxCdr.sl();
	psxRcntSl();
	mdecSl();
	psxGpu->sl();
	psxSpu->sl();

	if (!emsl.save) {
		new_dyna_restore();
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2)
		return -1;
    QGuiApplication *app = new QGuiApplication(argc, argv);
    QQuickView *view = new QQuickView();

	EmuView view(&psxEmu, argv[1], view);
	return app.exec();
}
