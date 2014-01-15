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

#ifndef HOSTVIDEO_H
#define HOSTVIDEO_H

class Emu;
class EmuThread;
class HostInput;
#include "base_global.h"
#include <QGLWidget>
#include <QTime>
#include <QThread>
#include "glpainter.h"

class BASE_EXPORT HostVideo : public QGLWidget
{
	Q_OBJECT
public:
	explicit HostVideo(HostInput *hostInput,
					   Emu *emu,
					   EmuThread *thread,
					   QWidget *parent = 0);
	~HostVideo();

	bool isFpsVisible() const;
	void setFpsVisible(bool on);

	bool keepApsectRatio() const;
	void setKeepAspectRatio(bool on);

	QRectF dstRect() const;

	QPoint convertCoordHostToEmu(const QPoint &hostPos);

    void startRendering();
    void stopRendering();

	HostInput *m_hostInput;
	Emu *m_emu;
	EmuThread *m_thread;

	QRectF m_srcRect;
	QRectF m_dstRect;

	bool m_fpsVisible;
	int m_fpsCount;
	int m_fpsCounter;
	QTime m_fpsCounterTime;

	bool m_keepAspectRatio;

    void updateFrame(QImage frame);
signals:
    void videoFrameChanged(QImage frame);

protected:
	void paintEvent(QPaintEvent *);
    void resizeEvent(QResizeEvent *event);

private slots:
	void updateRects();

private:
    GLPainter glPainter;
    QThread glThread;
};

inline QRectF HostVideo::dstRect() const
{ return m_dstRect; }
inline bool HostVideo::isFpsVisible() const
{ return m_fpsVisible; }
inline bool HostVideo::keepApsectRatio() const
{ return m_keepAspectRatio; }

#endif // HOSTVIDEO_H
