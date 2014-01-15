/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	(c) Copyright 2011, elemental
*/

#ifndef PICOCART_H
#define PICOCART_H

#include <base/emu.h>

class PicoCart {
public:
	PicoCart();
	~PicoCart();
	bool open(const QString &fileName, QString *error);
	bool openMegaCDBios(const QString &fileName, QString *error);
	void close();
private:
	void decodeSmd();
	bool allocateMem();
	void detect();
};

extern PicoCart picoCart;

#endif // PICOCART_H
