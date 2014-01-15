#ifndef GBCPUMAPPER_H
#define GBCPUMAPPER_H

class GBCpu;
#include <QObject>

class GBCpuMapper : public QObject {
    Q_OBJECT
public:
	virtual void reset();

	quint8 readOpcode(quint16 address);

	void write(quint16 address, quint8 data);
	quint8 read(quint16 address);
	quint8 readCheated(quint16 address);

	void writeSlowPath(quint16 address, quint8 data);
	quint8 readSlowPath(quint16 address, quint8 data);

	virtual void writeLow(quint16 address, quint8 data);

	virtual void writeExRam(quint16 address, quint8 data);
	virtual quint8 readExRam(quint16 address);
protected:
	quint8 *m_rom;
private:
	void writeIO(quint8 address, quint8 data);
	quint8 readIO(quint8 address);

	GBCpu *m_cpu;
	quint8 *m_rbanks[16];
	quint8 *m_wbanks[16];
};

inline void GBCpuMapper::write(quint16 address, quint8 data) {
	quint8 *bank = m_wbanks[address >> 12];
	if (bank)
		bank[address] = data;
	else
		writeSlowPath(address, data);
}

inline quint8 GBCpuMapper::read(int address) {
	quint8 *bank = m_rbanks[address >> 12];
	if (bank)
		return bank[address];
	else
		return readSlowPath(address);
}

inline quint8 GBCpuMapper::readCheated(quint16 address) {
//	TODO cheats if (gbCheatMap[address])
//		return gbCheatRead(address);
	return read(address);
}

inline void GBCpuMapper::writeWord(quint16 address, quint16 data) {
	if ((address+1) & 0x0FFF) {
		quint8 *bank = m_wbanks[address>>12];
		if (bank) {
			if (address & 1) {
				bank[address] = data;
				bank[address+1] = data >> 8;
			} else {
				*(quint16 *)(bank+address) = data;
			}
		}
	} else {
		writeSlowPath(address, data);
		writeSlowPath(address+1, data>>8);
	}
}

inline int GBCpuMapper::readWord(quint16 address) {
	if ((address+1) & 0x0FFF) {
		quint8 *bank = m_rbanks[address >> 12];
		if (bank) {
			if (address & 1)
				return bank[address] | (bank[address+1]<<8);
			return *(quint16 *)(bank+address);
		}
	}
	return readSlowPath(address) | (readSlowPath(address+1)<<8);
}

#endif // GBCPUMAPPER_H
