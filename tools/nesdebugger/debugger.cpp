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

#include "debugger.h"
#include <nes/mapper.h>
#include <QHostAddress>
#include <stdio.h>

NesDebugger::NesDebugger(QObject *parent) :
	QAbstractListModel(parent),
	m_profiler(m_profilerItems, this),
	m_disasm(m_memory)
{
	QHash<int, QByteArray> roles;
	roles.insert(DisasmRole, "disasm");
	roles.insert(BkptRole, "bkpt");
	roles.insert(CmntRole, "cmnt");
	roles.insert(PcRole, "pc");
	setRoleNames(roles);

	m_logEnabled = false;
	m_stepIntoIssued = false;
	m_stepOverIssued = false;

	m_breakOnNmi = false;

	m_currentIndexOfPcDirty = true;
	m_breakpointLastTime = false;

	m_profilerData = 0;
}

void NesDebugger::connectToServer()
{
	m_logFile.setFileName(QString("%1/Desktop/nes.log").arg(getenv("HOME")));
	m_logFile.open(QIODevice::WriteOnly|QIODevice::Truncate);

	for (int i = 0; i < 8; i++) {
		m_currentCpuBanks[i] = 0xff;
		m_lastCpuBanks[i] = 0xff;
	}

	m_sock = new QTcpSocket(this);
	static const char *peerAddr = "192.168.2.15";
	m_sock->connectToHost(QHostAddress(peerAddr), DefaultPort);
	if (!m_sock->waitForConnected()) {
		qDebug("Could not connect to the client at %s!", peerAddr);
		abort();
	}

	m_stream = new QDataStream(m_sock);
	m_stream->setByteOrder(QDataStream::LittleEndian);

	receivePc();

	fetchRom();
	fetchCpuBanks();
//	fetchCpuRegisters();
//	do not fetch registers on start !!!
	m_regs.a = 0;
	m_regs.x = 0;
	m_regs.y = 0;
	m_regs.s = 0xff;
	m_regs.p = NesCpuBase::IrqDisable;

	checkDisasm();

	emit changed();
}

void NesDebugger::setLogEnabled(bool on)
{
	if (m_logEnabled != on) {
		m_logEnabled = on;
		emit logEnableChanged();
	}
}

void NesDebugger::setBreakOnNmiEnabled(bool on)
{
	if (m_breakOnNmi != on) {
		m_breakOnNmi = on;
		emit breakOnNmiEnableChanged();
	}
}

int NesDebugger::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return m_instrPositions.size();
}

QVariant NesDebugger::data(const QModelIndex &index, int role) const
{
	u16 pc = m_instrPositions.at(index.row());
	switch (role) {
	case DisasmRole:
		return QString(m_disasm.instruction(pc).disassembled);
	case CmntRole:
		return QString(m_disasm.instruction(pc).comment);
	case BkptRole:
		return m_breakpoints.contains(pc);
	case PcRole:
		return QString("%1:").arg(pc, 4, 16, QLatin1Char('0'));
	}
	return QVariant();
}

int NesDebugger::currentIndexOfPc()
{
	if (m_currentIndexOfPcDirty) {
		m_currentIndexOfPcDirty = false;
		m_currentIndexOfPc = indexOf(m_regs.pc);
	}
	return m_currentIndexOfPc;
}

QString NesDebugger::bankString() const
{
	return QString("%1 %2 %3 %4")
			.arg(m_currentCpuBanks[4])
			.arg(m_currentCpuBanks[5])
			.arg(m_currentCpuBanks[6])
			.arg(m_currentCpuBanks[7]);
}

void NesDebugger::continueRun()
{
	if (!m_breakpoints.isEmpty() || m_breakOnNmi)
		waitForBreakpoint();
}

void NesDebugger::stepInto()
{
	m_stepIntoIssued = true;
	waitForBreakpoint();
}

void NesDebugger::stepOver()
{
	u16 next = m_disasm.nextInstruction(m_regs.pc);
	bool was = m_breakpoints.contains(next);
	insertBreakpointPc(next);
	if (!was)
		m_breakpoints.remove(next);
	waitForBreakpoint();
}

void NesDebugger::insertBreakpoint(int pos)
{
	u16 pc = m_instrPositions.at(pos);
	insertBreakpointPc(pc);
	emit dataChanged(index(pos), index(pos));
}

void NesDebugger::removeBreakpoint(int pos)
{
	u16 pc = m_instrPositions.at(pos);
	removeBreakpointPc(pc);
	emit dataChanged(index(pos), index(pos));
}

int NesDebugger::indexOf(int pc) const
{
	return m_instrPositions.indexOf(pc);
}

void NesDebugger::toggleBreakpoint(int pos)
{
	u16 pc = m_instrPositions.at(pos);
	if (m_breakpoints.contains(pc))
		removeBreakpoint(pos);
	else
		insertBreakpoint(pos);
}

void NesDebugger::fetchProfiler()
{
	u8 cmd = SendProfiler;
	*m_stream << cmd;
	int size = m_romSize * sizeof(int);
	if (!m_profilerData)
		m_profilerData = new int[m_romSize];
	waitForBytes(size);
	m_stream->readRawData((char *)m_profilerData, size);

	processProfiler();

	m_profiler.reset();
}

void NesDebugger::resetProfiler()
{
	u8 cmd = ClearProfiler;
	*m_stream << cmd;

	m_profilerItems.clear();
	m_profiler.reset();
}

void NesDebugger::insertBreakpointPc(u16 pc)
{
	u8 cmd = InsertBreakpoint;
	*m_stream << cmd;
	*m_stream << pc;
	m_breakpoints.insert(pc);
}

void NesDebugger::removeBreakpointPc(u16 pc)
{
	u8 cmd = RemoveBreakpoint;
	*m_stream << cmd;
	*m_stream << pc;
	m_breakpoints.remove(pc);
}

void NesDebugger::fetchCpuRegisters()
{
	u8 cmd = SendCpuRegisters;
	*m_stream << cmd;
	waitForBytes(5);
	*m_stream >> m_regs.a;
	*m_stream >> m_regs.x;
	*m_stream >> m_regs.y;
	*m_stream >> m_regs.s;
	*m_stream >> m_regs.p;
}

void NesDebugger::fetchCpuBanks()
{
	u8 cmd = SendCpuBanks;
	*m_stream << cmd;
	waitForBytes(8);
	m_stream->readRawData((char *)m_currentCpuBanks, 8);
}

void NesDebugger::fetchRom()
{
	u8 cmd = SendRomMemory;
	*m_stream << cmd;
	waitForBytes(4);
	*m_stream >> m_romSize;
	m_rom = new u8[m_romSize];
	waitForBytes(m_romSize);
	m_stream->readRawData((char *)m_rom, m_romSize);
}

void NesDebugger::receivePc()
{
	waitForBytes(2);
	*m_stream >> m_regs.pc;
	m_currentIndexOfPcDirty = true;
}

void NesDebugger::sendMask(int mask)
{
	u8 cmd = SetEventMask;
	*m_stream << cmd;
	*m_stream << mask;
}

void NesDebugger::waitForBreakpoint()
{
	int mask = 1 << DebugBankSwitch;;
	if (m_logEnabled) {
		mask |= 1 << DebugCpuOp;
		mask |= 1 << DebugNmi;
		mask |= 1 << DebugIrq;
		mask |= 1 << DebugBreakpoint;
	} else {
		if (m_breakOnNmi)
			mask |= 1 << DebugNmi;
		if (m_stepIntoIssued)
			mask |= 1 << DebugCpuOp;
		mask |= 1 << DebugBreakpoint;
	}
	sendMask(mask);

	u8 cmd = Continue;
	*m_stream << cmd;

	for (;;) {
		waitForBytes(1);
		u8 ev;
		*m_stream >> ev;

		if (m_sock->state() != QTcpSocket::ConnectedState)
			return;

		if (ev == DebugBreakpoint) {
			m_breakpointLastTime = true;
			receivePc();
			break;
		} else {
			bool wasBreak = m_breakpointLastTime;
			m_breakpointLastTime = false;
			if (ev == DebugCpuOp) {
				receivePc();
			} else if (ev == DebugBankSwitch) {
				waitForBytes(2);
				u8 page;
				u8 bank;
				*m_stream >> page >> bank;
				m_currentCpuBanks[page] = bank;
			}

			if (m_logEnabled)
				logEvent(static_cast<DebugEvent>(ev));

			if (ev == DebugCpuOp) {
				if (m_stepIntoIssued && !wasBreak) {
					m_logFile.flush();
					m_stepIntoIssued = false;
					break;
				}
			} else if (ev == DebugNmi) {
				if (m_breakOnNmi)
					break;
			}
		}
		u8 cmd = Continue;
		*m_stream << cmd;
	}

	if (m_stepOverIssued) {
		m_logFile.flush();
		m_stepOverIssued = false;
		if (!m_breakpoints.contains(m_regs.pc))
			removeBreakpointPc(m_regs.pc);
	}

	fetchCpuRegisters();
	fetchCpuBanks();
	checkDisasm();

	emit changed();
}

void NesDebugger::checkDisasm()
{
	bool dirty = false;
	for (int i = 4; i < 8; i++) {
		if (m_lastCpuBanks[i] != m_currentCpuBanks[i]) {
			memcpy(m_memory + i * NesCpuBankSize,
				   m_rom + m_currentCpuBanks[i] * NesCpuBankSize,
				   NesCpuBankSize);
			dirty = true;
			m_lastCpuBanks[i] = m_currentCpuBanks[i];
		}
	}

	if (dirty) {
		m_disasm.clear();
		m_disasm.instruction(m_regs.pc);
		m_instrPositions = m_disasm.positions();
		m_currentIndexOfPcDirty = true;
		reset();
	}
}

void NesDebugger::logEvent(DebugEvent ev)
{
	char buf[64];
	if (ev == DebugCpuOp) {
		fetchCpuRegisters();
		checkDisasm();
		NesInstruction instr = m_disasm.instruction(m_regs.pc);
		sprintf(buf, "%04x: %02x %02x %02x %02x %02x %02x\n",
				m_regs.pc, instr.op, m_regs.a, m_regs.x, m_regs.y, m_regs.s, m_regs.p);
		m_logFile.write(buf);
	} else if (ev == DebugNmi) {
		m_logFile.write("NMI\n");
	} else if (ev == DebugIrq) {
		m_logFile.write("IRQ\n");
	} else if (ev == DebugBankSwitch) {
		m_logFile.write("BANK\n");
	}
}

void NesDebugger::waitForBytes(int n)
{
	while (n > m_sock->bytesAvailable()) {
		if (!m_sock->waitForReadyRead(-1)) {
			m_logFile.flush();
			abort();
		}
	}
}

void NesDebugger::processProfiler()
{
	m_profilerItems.clear();
	processProfilerPage(4);
	processProfilerPage(5);
	processProfilerPage(6);
	processProfilerPage(7);
	qSort(m_profilerItems);
}

void NesDebugger::processProfilerPage(int i)
{
	int bank = m_currentCpuBanks[i] * NesCpuBankSize;
	u16 start = i * NesCpuBankSize;
	u16 end = start + NesCpuBankSize;
	for (u16 offset = start; offset != end; offset++) {
		int count = m_profilerData[bank + (offset&NesCpuBankMask)];
		if (count) {
			ProfilerItem item;
			item.first = offset;
			item.second = count;
			m_profilerItems.append(item);
		}
	}
}
