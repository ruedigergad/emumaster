#include "gbserial.h"

GBSerial::GBSerial(QObject *parent) :
    QObject(parent)
{
}

void GBSerial::emulate() {
	if (!gbSerialOn)
		return;
	if (!(R->SC & ShiftClock))
		return;
	gbSerialTicks -= clockTicks;

	// overflow
	while (gbSerialTicks <= 0) {
		// increment number of shifted bits
		gbSerialBits++;
		if (gbSerialBits == 8) {
			// end of transmission
			if (gbSerialFunction) // external device
				R->SB = gbSerialFunction(R->SB);
			else
				R->SB = 0xff;
			gbSerialTicks = 0;
			R->SC &= ~TransferStart;
			gbSerialOn = 0;
			gbMemory[0xff0f] = register_IF |= 8;
			gbSerialBits	= 0;
		} else {
			gbSerialTicks += GBSERIAL_CLOCK_TICKS;
		}
	}
}
