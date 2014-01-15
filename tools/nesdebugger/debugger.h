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

#ifndef NESDEBUGGER_H
#define NESDEBUGGER_H

#include "disassembler.h"
#include "profiler.h"
#include <QAbstractListModel>
#include <QFile>
#include <QTcpSocket>
#include <QDataStream>
#include <QSet>
#include <base/emu.h>
#include <nes/cpubase.h>
#include <nes/debug.h>

class NesDebugger : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int regPC READ regPC NOTIFY changed)
	Q_PROPERTY(int regA READ regA NOTIFY changed)
	Q_PROPERTY(int regX READ regX NOTIFY changed)
	Q_PROPERTY(int regY READ regY NOTIFY changed)
	Q_PROPERTY(int regS READ regS NOTIFY changed)
	Q_PROPERTY(int regP READ regP NOTIFY changed)
	Q_PROPERTY(QString regPCx READ regPCx NOTIFY changed)
	Q_PROPERTY(QString regAx READ regAx NOTIFY changed)
	Q_PROPERTY(QString regXx READ regXx NOTIFY changed)
	Q_PROPERTY(QString regYx READ regYx NOTIFY changed)
	Q_PROPERTY(QString regSx READ regSx NOTIFY changed)
	Q_PROPERTY(QString regPx READ regPx NOTIFY changed)
	Q_PROPERTY(bool logEnable READ isLogEnabled WRITE setLogEnabled NOTIFY logEnableChanged)
	Q_PROPERTY(bool breakOnNmiEnable READ isBreakOnNmiEnabled WRITE setBreakOnNmiEnabled NOTIFY breakOnNmiEnableChanged)
	Q_PROPERTY(int currentIndexOfPc READ currentIndexOfPc NOTIFY changed)
	Q_PROPERTY(QString bankString READ bankString NOTIFY changed)
	Q_PROPERTY(NesProfiler *prof READ prof CONSTANT)
public:
	enum Role {
		DisasmRole = Qt::UserRole+1,
		CmntRole,
		BkptRole,
		PcRole
	};

	explicit NesDebugger(QObject *parent = 0);

	bool isLogEnabled() const { return m_logEnabled; }
	void setLogEnabled(bool on);

	bool isBreakOnNmiEnabled() const { return m_breakOnNmi; }
	void setBreakOnNmiEnabled(bool on);

	u16 regPC() const { return m_regs.pc; }
	u8 regA() const { return m_regs.a; }
	u8 regX() const { return m_regs.x; }
	u8 regY() const { return m_regs.y; }
	u8 regS() const { return m_regs.s; }
	u8 regP() const { return m_regs.p; }

	QString regPCx() const { return QString("%1").arg(m_regs.pc, 4, 16, QLatin1Char('0')); }
	QString regAx() const { return QString("%1").arg(m_regs.a, 2, 16, QLatin1Char('0')); }
	QString regXx() const { return QString("%1").arg(m_regs.x, 2, 16, QLatin1Char('0')); }
	QString regYx() const { return QString("%1").arg(m_regs.y, 2, 16, QLatin1Char('0')); }
	QString regSx() const { return QString("%1").arg(m_regs.s, 2, 16, QLatin1Char('0')); }
	QString regPx() const { return QString("%1").arg(m_regs.p, 2, 16, QLatin1Char('0')); }

	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	int currentIndexOfPc();

	QString bankString() const;

	NesProfiler *prof() { return &m_profiler; }
public slots:
	void connectToServer();
	void continueRun();
	void stepInto();
	void stepOver();
	void insertBreakpoint(int pos);
	void removeBreakpoint(int pos);
	int indexOf(int pc) const;
	void toggleBreakpoint(int pos);

	void fetchProfiler();
	void resetProfiler();
signals:
	void changed();
	void logEnableChanged();
	void breakOnNmiEnableChanged();
private:
	void insertBreakpointPc(u16 pc);
	void removeBreakpointPc(u16 pc);
	void fetchCpuRegisters();
	void fetchCpuBanks();
	void fetchRom();
	void receivePc();
	void sendMask(int mask);
	void waitForBreakpoint();
	void checkDisasm();
	void logEvent(DebugEvent ev);
	void waitForBytes(int n);

	void processProfiler();
	void processProfilerPage(int i);

	bool m_logEnabled;
	QFile m_logFile;
	QTcpSocket *m_sock;
	QDataStream *m_stream;

	u8 m_currentCpuBanks[8];
	u8 m_lastCpuBanks[8];
	u8 m_memory[0x10000];
	u8 *m_rom;
	uint m_romSize;
	NesCpuBaseRegisters m_regs;

	bool m_stepIntoIssued;
	bool m_stepOverIssued;
	u16 m_stepOverAddr;
	bool m_breakpointLastTime;

	QSet<u16> m_breakpoints;
	bool m_breakOnNmi;
	int m_currentIndexOfPc;
	bool m_currentIndexOfPcDirty;
	int *m_profilerData;
	QList<ProfilerItem> m_profilerItems;

	NesProfiler m_profiler;

	QList<u16> m_instrPositions;

	NesDisassembler m_disasm;
};

#endif // NESDEBUGGER_H
