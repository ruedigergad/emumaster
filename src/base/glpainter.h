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

#ifndef GLPAINTER_H
#define GLPAINTER_H

#include <QObject>
#include <QTimer>
#include <QPainter>
#include <QMutex>
#include <QThread>
#include <QTimerEvent>
#include <QPainter>

class HostVideo;

class GLPainter : public QObject
{
    Q_OBJECT
public:
    GLPainter(HostVideo *widget);
    void stop();
    void resizeViewport(const QSize &size);

public slots:
    void start();

    void paintFrame(QImage frame);

protected:
    void paint();

private:
    QMutex mutex;
    HostVideo *glWidget;
    int viewportWidth;
    int viewportHeight;
    bool doRendering;

    void paintFps(QPainter *painter);
};

#endif // GLPAINTER_H
