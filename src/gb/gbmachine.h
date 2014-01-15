#ifndef GBMACHINE_H
#define GBMACHINE_H

#include <imachine.h>

class GBMachine : public IMachine {
	Q_OBJECT
public:
	explicit GBMachine(QObject *parent = 0);
	void emulateFrame(bool drawEnabled);
};

#endif // GBMACHINE_H
