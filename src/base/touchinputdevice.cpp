/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "touchinputdevice.h"
#include "pathmanager.h"
#include "emu.h"
#include <QTouchEvent>
#include <QPainter>
#include <QDebug>

enum ButtonsInImage {
	Button_Settings,
	Button_Exit,
	Button_Left,
	Button_Up,
	Button_Right,
	Button_Down,
	Button_A,
	Button_B,
	Button_X,
	Button_Y,
	Button_Circle,
	Button_Square,
	Button_Triangle,
	Button_L1,
	Button_R1,
	Button_L2,
	Button_R2,
	Button_L,
	Button_R,
	Button_M,
	Button_Select,
	Button_Start,
	Button_Z,
	Button_C,
	Button_Cross = Button_X
};

TouchInputDevice::TouchInputDevice(QObject *parent) :
	HostInputDevice("touch", QObject::tr("Touch Screen"), parent),
	m_numPoints(0),
    m_areaSize(512),
    m_diagonalAreaSize(512/4),
	m_gridVisible(true),
	m_lrVisible(false),
	m_gridColor(Qt::white),
	m_buttonsVisible(true),
	m_psxButtonsEnable(false),
	m_picoButtonsEnable(false),
	m_gbaButtonsEnable(false)
{
	setupEmuFunctionList();

	QObject::connect(this, SIGNAL(emuFunctionChanged()), SLOT(onEmuFunctionChanged()));

	m_buttonsImage.load(pathManager.installationDirPath()+"/data/buttons.png");

    m_width = 640;
    m_height = 480;
}

//void TouchInputDevice::setHostVideo(HostVideo *hostVideo)
//{
//	m_hostVideo = hostVideo;
//}

void TouchInputDevice::processTouch(QEvent *e)
{
	m_numPoints = 0;

	QTouchEvent *touchEvent = static_cast<QTouchEvent *>(e);
	QList<QTouchEvent::TouchPoint> points = touchEvent->touchPoints();

	for (int i = 0; i < points.size(); i++) {
		QTouchEvent::TouchPoint point = points.at(i);
		if (point.state() & Qt::TouchPointReleased)
			continue;

		m_points[m_numPoints] = point.pos().toPoint();
		m_numPoints++;
		if (m_numPoints >= MaxPoints)
			break;
	}

	m_converted = false;
}

void TouchInputDevice::onEmuFunctionChanged()
{
	m_converted = false;
	m_numPoints = 0;
	m_buttons = 0;
	m_mouseX = m_mouseY = 0;

	updateGrid();
	updatePaintedButtons();
}

void TouchInputDevice::setupEmuFunctionList()
{
	QStringList functionNameList;
	functionNameList << tr("None")
					 << tr("Pad A")
					 << tr("Pad B")
					 << tr("Mouse A")
					 << tr("Mouse B")
					 << tr("Touch");
	setEmuFunctionNameList(functionNameList);
}

void TouchInputDevice::sync(EmuInput *emuInput)
{
	if (emuFunction() <= 0)
		return;

	if (emuFunction() <= 2) { // Pad
		if (!m_converted) {
			convertPad();
			m_converted = true;
		}
		int padIndex = emuFunction() - 1;
		emuInput->pad[padIndex].setButtons(m_buttons);
	} else if (emuFunction() <= 4) { // Mouse
		if (!m_converted) {
			convertMouse();
			m_converted = true;
		}
		int xRel = m_mouseX - m_lastMouseX;
		int yRel = m_mouseY - m_lastMouseY;
		int mouseIndex = emuFunction() - 3;
		emuInput->mouse[mouseIndex].setButtons(m_buttons);
		emuInput->mouse[mouseIndex].addRel(xRel, yRel);
	} else if (emuFunction() == 5) { // Touch
		if (!m_converted) {
			convertTouch();
			m_converted = true;
		}
		emuInput->touch.setPos(m_touchPointInEmu.x(), m_touchPointInEmu.y());
	}
}

void TouchInputDevice::convertPad()
{
	int oldButtons = m_buttons;
	m_buttons = 0;

	for (int i = 0; i < m_numPoints; i++) {
		int x = m_points[i].x();
		int y = m_points[i].y();
		if (y >= m_height-m_areaSize) {
			y -= m_height-m_areaSize;
			if (x < m_areaSize) {
				// directions
				m_buttons |= buttonsInDpad(x, y);
			} else if (x >= m_width-m_areaSize) {
				// a,b,x,y
				x -= m_width-m_areaSize;
				m_buttons |= buttonsInDpad(x, y) << 4;
			} else if (x >= m_width/2-ButtonWidth &&
					   x < m_width/2+ButtonWidth) {
				// select, start
				if (y >= m_areaSize-ButtonHeight) {
					if (x < m_width/2)
						m_buttons |= EmuPad::Button_Select;
					else
						m_buttons |= EmuPad::Button_Start;
				}
			}
		} else if (y >= m_lrYPos && y < m_lrYPos+ButtonHeight) {
			// l1,l2,r1,r2
			if (x < ButtonWidth)
				m_buttons |= EmuPad::Button_L1;
			else if (x < ButtonWidth*2)
				m_buttons |= EmuPad::Button_L2;
			else if (x >= m_width-ButtonWidth)
				m_buttons |= EmuPad::Button_R1;
			else if (x >= m_width-ButtonWidth*2)
				m_buttons |= EmuPad::Button_R2;
		}
	}
//	if (m_hapticEffect) {
		// start feedback when new button is pressed
//		if ((m_buttons & oldButtons) != m_buttons)
//			m_hapticEffect->start();
//	}
}

void TouchInputDevice::convertMouse()
{
	m_buttons = 0;
	m_lastMouseX = m_mouseX;
	m_lastMouseY = m_mouseY;

	bool newMoving = false;
	for (int i = 0; i < m_numPoints; i++) {
		int x = m_points[i].x();
		int y = m_points[i].y();
		if (y >= m_height-m_areaSize) {
			y -= m_height-m_areaSize;
			if (x < m_areaSize) {
				m_mouseX = x - m_areaSize/2;
				m_mouseY = y - m_areaSize/2;
				// when moving started
				if (!m_mouseMoving) {
					m_lastMouseX = m_mouseX;
					m_lastMouseY = m_mouseY;
				}
				newMoving = true;
			} else if (x >= m_width-m_areaSize) {
				x -= m_width-m_areaSize;
				int buttons = buttonsInDpad(x, y);
				// swap bits
				int left   = (buttons & 2) >> 1;
				int right  = (buttons & 0) >> 0;
				int middle = (buttons & 4) >> 2;
				m_buttons |= (left << 0) | (right << 1) | (middle << 2);
			}
		}
	}
	m_mouseMoving = newMoving;
}

void TouchInputDevice::convertTouch()
{
	if (m_numPoints <= 0)
		m_touchPointInEmu = QPoint(-1, -1);
//	else
//		m_touchPointInEmu = m_hostVideo->convertCoordHostToEmu(m_points[0]);
}

int TouchInputDevice::buttonsInDpad(int x, int y) const
{
	int buttons = 0;
	if (x < m_diagonalAreaSize) {
		buttons |= EmuPad::Button_Left;
		if (y < m_diagonalAreaSize)
			buttons |= EmuPad::Button_Up;
		else if (y >= m_areaSize-m_diagonalAreaSize)
			buttons |= EmuPad::Button_Down;
	} else if (x >= m_areaSize-m_diagonalAreaSize) {
		buttons |= EmuPad::Button_Right;
		if (y < m_diagonalAreaSize)
			buttons |= EmuPad::Button_Up;
		else if (y >= m_areaSize-m_diagonalAreaSize)
			buttons |= EmuPad::Button_Down;
	} else {
		if (y < m_diagonalAreaSize) {
			buttons |= EmuPad::Button_Up;
		} else if (y >= m_areaSize-m_diagonalAreaSize) {
			buttons |= EmuPad::Button_Down;
		} else {
			x -= m_areaSize/2;
			y -= m_areaSize/2;
			if (qAbs(x) > qAbs(y)) {
				if (x > 0)
					buttons |= EmuPad::Button_Right;
				else
					buttons |= EmuPad::Button_Left;
			} else {
				if (y > 0)
					buttons |= EmuPad::Button_Down;
				else
					buttons |= EmuPad::Button_Up;
			}
		}
	}
	return buttons;
}

void TouchInputDevice::paint(QPainter *painter)
{
	QVector<PaintedButton>::ConstIterator i = m_paintedButtons.constBegin();
	for (; i != m_paintedButtons.constEnd(); i++) {
		if (m_buttons & i->flag)
			painter->drawImage(i->dst, m_buttonsImage, i->srcPressed);
		else
			painter->drawImage(i->dst, m_buttonsImage, i->src);
	}
	if (!m_grid.isEmpty())
		painter->strokePath(m_grid, QPen(m_gridColor));
}

void TouchInputDevice::setHapticFeedbackEnabled(bool on)
{
//	if (on == (m_hapticEffect != 0))
//		return;
//	if (on) {
//		m_hapticEffect = new QFeedbackHapticsEffect(this);
//		m_hapticEffect->setIntensity(0.25f);
//		m_hapticEffect->setDuration(30);
//	} else {
//		delete m_hapticEffect;
//		m_hapticEffect = 0;
//	}
}

void TouchInputDevice::setLRVisible(bool on)
{
	m_lrVisible = on;
	updateGrid();
	updatePaintedButtons();
}

void TouchInputDevice::setGridVisible(bool on)
{
	if (m_gridVisible != on) {
		m_gridVisible = on;
		updateGrid();
		emit gridVisibleChanged();
	}
}

void TouchInputDevice::setDpadAreaSize(int size)
{
	int rSize = qBound(160, size, 320);
	if (m_areaSize != rSize) {
		m_areaSize = rSize;
		setDpadAreaDiagonalSize(m_diagonalAreaSize);
		updateGrid();
		updatePaintedButtons();
		emit dpadAreaSizeChanged();
	}
}

void TouchInputDevice::setDpadAreaDiagonalSize(int size)
{
	int rSize = qBound(0, size, m_areaSize/2);
	if (m_diagonalAreaSize != rSize) {
		m_diagonalAreaSize = rSize;
		updateGrid();
		updatePaintedButtons();
		emit dpadAreaDiagonalSizeChanged();
	}
}

void TouchInputDevice::setGridColor(const QColor &color)
{
	if (m_gridColor != color) {
		m_gridColor = color;
		updateGrid();
		emit gridColorChanged();
	}
}

void TouchInputDevice::setButtonsVisible(bool on)
{
	if (m_buttonsVisible != on) {
		m_buttonsVisible = on;
		updatePaintedButtons();
		emit buttonsVisibleChanged();
	}
}

void TouchInputDevice::setPsxButtonsEnabled(bool on)
{
	m_psxButtonsEnable = on;
	updatePaintedButtons();
}

void TouchInputDevice::setPicoButtonsEnabled(bool on)
{
	m_picoButtonsEnable = on;
	updatePaintedButtons();
}

void TouchInputDevice::setGbaButtonsEnabled(bool on)
{
	m_gbaButtonsEnable = on;
	updatePaintedButtons();
}

void TouchInputDevice::updatePaintedButtons()
{
	m_paintedButtons.clear();

	if (!m_buttonsVisible)
		return;
	if (emuFunction() <= 0)
		return;

	// settings,exit
//	addPaintedButton(Button_Settings, 0, QPointF(0, 0));
//	addPaintedButton(Button_Exit, 0, QPointF(m_width-PaintedButtonSize, 0));

	if (emuFunction() <= 2)
		addPaintedButtonsPad();
	else if (emuFunction() <= 4)
		addPaintedButtonsMouse();
}

void TouchInputDevice::addPaintedButtonsPad()
{
	// left dpad
	QPointF leftPos(10, m_height-m_areaSize/2-PaintedButtonSize/2);
	addPaintedButton(Button_Left, EmuPad::Button_Left, leftPos);
	QPointF rightPos(m_areaSize-PaintedButtonSize-10, leftPos.y());
	addPaintedButton(Button_Right, EmuPad::Button_Right, rightPos);
	QPointF upPos(m_areaSize/2-PaintedButtonSize/2,
				  m_height-m_areaSize+10);
	addPaintedButton(Button_Up, EmuPad::Button_Up, upPos);
	QPointF downPos(upPos.x(), m_height-PaintedButtonSize-10);
	addPaintedButton(Button_Down, EmuPad::Button_Down, downPos);
	// buttons at the right
	QPointF offset(m_width-m_areaSize, 0);
	if (!m_gbaButtonsEnable) {
		if (!m_picoButtonsEnable) {
			addPaintedButton(m_psxButtonsEnable ? Button_Square : Button_Y,
							 EmuPad::Button_Y, leftPos + offset);
			addPaintedButton(m_psxButtonsEnable ? Button_Triangle : Button_X,
							 EmuPad::Button_X, upPos + offset);
		} else {
			addPaintedButton(Button_X, EmuPad::Button_Y, leftPos + offset);
			addPaintedButton(Button_C, EmuPad::Button_X, upPos + offset);
		}
	}
	addPaintedButton(m_psxButtonsEnable ? Button_Circle : Button_A,
					 EmuPad::Button_A, rightPos + offset);
	addPaintedButton(m_psxButtonsEnable ? Button_Cross : Button_B,
					 EmuPad::Button_B, downPos + offset);
	// l1,r1,l2,r2
	QPointF lPos(ButtonWidth/2-PaintedButtonSize/2, m_lrYPos);
	QPointF rPos = lPos + QPointF(m_width-ButtonWidth, 0);
	QPointF l2Pos = lPos + QPointF(ButtonWidth, 0);
	QPointF r2Pos = rPos - QPointF(ButtonWidth, 0);
	if (m_lrVisible) {
		bool l2r2Visible = m_psxButtonsEnable;
		if (!m_picoButtonsEnable) {
			addPaintedButton(l2r2Visible ? Button_L1 : Button_L,
							 EmuPad::Button_L1, lPos);
			addPaintedButton(l2r2Visible ? Button_R1 : Button_R,
							 EmuPad::Button_R1, rPos);
		} else {
			addPaintedButton(Button_Y, EmuPad::Button_L1, lPos);
			addPaintedButton(Button_Z, EmuPad::Button_R1, rPos);
		}
		if (l2r2Visible) {
			addPaintedButton(Button_L2, EmuPad::Button_L2, l2Pos);
			addPaintedButton(Button_R2, EmuPad::Button_R2, r2Pos);
		}
	}
	// select,start
	QPointF selectPos(m_width/2-ButtonWidth/2-PaintedButtonSize/2,
					  m_height-ButtonHeight/2-PaintedButtonSize/2);
	QPointF startPos = selectPos + QPointF(ButtonWidth, 0);
	addPaintedButton(Button_Select, EmuPad::Button_Select, selectPos);
	addPaintedButton(Button_Start, EmuPad::Button_Start, startPos);
}

void TouchInputDevice::addPaintedButtonsMouse()
{
	QPointF leftPos(10, m_height-m_areaSize/2-PaintedButtonSize/2);
	QPointF rightPos(m_areaSize-PaintedButtonSize-10, leftPos.y());
	QPointF upPos(m_areaSize/2-PaintedButtonSize/2,
				  m_height-m_areaSize+10);
	QPointF downPos(upPos.x(), m_height-PaintedButtonSize-10);
	// buttons at the right
	QPointF offset(m_width-m_areaSize, 0);

	addPaintedButton(Button_L, EmuMouse::Button_Left, downPos + offset);
	addPaintedButton(Button_R, EmuMouse::Button_Right, rightPos + offset);
	addPaintedButton(Button_M, EmuMouse::Button_Middle, upPos + offset);
}

void TouchInputDevice::addPaintedButton(int button, int flag, QPointF pos)
{
	int n = 512/64;
	int row = button / n;
	int column = button % n;

	int y = row * 128;
	int x = column * 64;
	PaintedButton paintedButton;

	paintedButton.flag = flag;
	paintedButton.src = QRectF(x, y, 64, 64);
	paintedButton.srcPressed = QRectF(x, y+64, 64, 64);
	paintedButton.dst = pos;

	m_paintedButtons.append(paintedButton);
}

void TouchInputDevice::updateGrid()
{
	m_lrYPos = ButtonHeight/2+(m_height-m_areaSize-ButtonHeight)/2;
	m_grid = QPainterPath();

	if (emuFunction() <= 0)
		return;

	// add "touchpad" rect even if grid is not visible
	if (emuFunction() >= 3 && emuFunction() <= 4 && (m_gridVisible || m_buttonsVisible)) {
		// left "touchpad"
		QPointF p1(0, m_height-m_areaSize);
		QSizeF sizeArea(m_areaSize-1, m_areaSize-1);
		m_grid.addRect(QRectF(p1, sizeArea));
	}

	if (!m_gridVisible)
		return;

	QSizeF sizeButton(ButtonWidth-1, ButtonHeight-1);
	// settings button
	m_grid.addRect(QRectF(QPointF(0, 0), sizeButton));
	// exit button
	m_grid.addRect(QRectF(QPointF(m_width-ButtonWidth, 0), sizeButton));

	if (emuFunction() <= 2) {
		addGridPad();
	} else if (emuFunction() <= 4) {
		// l,r,m mouse buttons
		addDpadAreaToGrid(m_width-m_areaSize,
						  m_height-m_areaSize);
	}
}

void TouchInputDevice::addGridPad()
{
	QSizeF sizeButton(ButtonWidth-1, ButtonHeight-1);
	// left dpad
	addDpadAreaToGrid(0, m_height-m_areaSize);
	// right dpad
	addDpadAreaToGrid(m_width-m_areaSize, m_height-m_areaSize);

	// start,select
	QRectF startRect(QPointF(m_width/2-ButtonWidth,
							 m_height-ButtonHeight),
					 sizeButton);
	QRectF selectRect = startRect;
	selectRect.moveLeft(m_width/2);
	m_grid.addRect(startRect);
	m_grid.addRect(selectRect);

	// l1,r1
	if (m_lrVisible) {
		QRectF l1Rect(QPointF(0, m_lrYPos), sizeButton);
		QRectF r1Rect = l1Rect;
		r1Rect.moveRight(m_width-1);
		m_grid.addRect(l1Rect);
		m_grid.addRect(r1Rect);

		bool l2r2Visible = (m_lrVisible && m_psxButtonsEnable);
		// l2,r2
		if (l2r2Visible) {
			QRectF l2Rect(QPointF(ButtonWidth, m_lrYPos), sizeButton);
			QRectF r2Rect = l2Rect;
			r2Rect.moveRight(m_width-ButtonWidth-1);
			m_grid.addRect(l2Rect);
			m_grid.addRect(r2Rect);
		}
	}
}

// p1-----------------p2------
// |    |             |      |
// |    |             |      |
// -----p5            p6------
// |     \            /      |
// |       \        /        |
// |         \    /          |
// |           \/            |
// |           /\            |
// |         /    \          |
// |       /        \        |
// |     /            \      |
// p3---p7             p4-----
// |     |             |     |
// |     |             |     |
// ---------------------------
void TouchInputDevice::addDpadAreaToGrid(int x, int y)
{
	QPointF p1(x, y);
	QPointF p2(x+m_areaSize-m_diagonalAreaSize, y);
	QPointF p3(x, y+m_areaSize-m_diagonalAreaSize);
	QPointF p4(x+m_areaSize-m_diagonalAreaSize, y+m_areaSize-m_diagonalAreaSize);
	QPointF p5(x+m_diagonalAreaSize, y+m_diagonalAreaSize);
	QPointF p6(x+m_areaSize-m_diagonalAreaSize, y+m_diagonalAreaSize);
	QPointF p7(x+m_diagonalAreaSize, y+m_areaSize-m_diagonalAreaSize);

	QSizeF sizeArea(m_areaSize-1, m_areaSize-1);
	QSizeF sizeDiagonal(m_diagonalAreaSize-1, m_diagonalAreaSize-1);

	m_grid.addRect(QRectF(p1, sizeArea));
	m_grid.addRect(QRectF(p1, sizeDiagonal));
	m_grid.addRect(QRectF(p2, sizeDiagonal));
	m_grid.addRect(QRectF(p3, sizeDiagonal));
	m_grid.addRect(QRectF(p4, sizeDiagonal));
	m_grid.moveTo(p5);
	m_grid.lineTo(p4);
	m_grid.moveTo(p7);
	m_grid.lineTo(p6);
}
