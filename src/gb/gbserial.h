#ifndef GBSERIAL_H
#define GBSERIAL_H

#include <QObject>

class GBSerial : public QObject {
	Q_OBJECT
public:
	enum ControlBit {
		TransferStart		= 0x80,
		ClockSpeed			= 0x02,
		ShiftClock			= 0x01
	};

	explicit GBSerial(QObject *parent = 0);
	void emulate();
private:
	struct {
		quint8 SB;
		quint8 SC;
	} *R;
};

#endif // GBSERIAL_H
