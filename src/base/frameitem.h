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

#ifndef FRAMEITEM_H
#define FRAMEITEM_H

#include <QImage>
#include <QPainter>
#include <QQuickItem>
#include <QTime>
#include <QTouchEvent>

class EmuView;

class FrameItem : public QQuickItem
{
    Q_OBJECT

public:
    FrameItem(QQuickItem *parent = 0);

    QSGNode* updatePaintNode(QSGNode *, UpdatePaintNodeData *);

    Q_INVOKABLE void setEmuView(QObject *emuView);
    
signals:
    void fpsUpdated(int fps);

public slots:
    void handleNewFrame(QImage frame);

protected:
    void touchEvent(QTouchEvent * touchEvent);

private:
    EmuView *m_emuView;
    QImage m_currentFrame;
	bool m_keepAspectRatio;

	bool m_fpsVisible;
	int m_fpsCount;
	int m_fpsCounter;
	QTime m_fpsCounterTime;

    void updateFps();
};

#endif // FRAMEITEM_H
