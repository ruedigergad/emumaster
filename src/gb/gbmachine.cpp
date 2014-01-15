#include "gbmachine.h"

GBMachine::GBMachine(QObject *parent) :
	QObject(parent)
{
}

void GBMachine::emulateFrame(bool drawEnabled) {
	m_lcd->setDrawEnabled(drawEnabled);
	m_cpu->emulate(2280);
	while (m_lcd->currentScanline() < 144)
		m_cpu->emulate(m_lcd->nextCycles());
	if (m_lcd->isEnabled())
		m_cpu->emulate(32832);
	while (m_lcd->currentScanline() > 0)
		m_cpu->emulate(m_lcd->nextCycles());
}
