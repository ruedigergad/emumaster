#include <QDebug>
#include "glpainter.h"
#include "hostvideo.h"
#include "emu.h"
#include "emuthread.h"
#include "hostinput.h"

GLPainter::GLPainter(HostVideo *widget) :
    glWidget(widget),
    doRendering(true)
    {
    }

void GLPainter::start()
{
    qDebug("Starting glPainter");
//    glWidget->makeCurrent();
//    startTimer(20);
}

void GLPainter::stop()
{
    QMutexLocker locker(&mutex);
    doRendering = false;
}

void GLPainter::resizeViewport(const QSize &size)
{
    QMutexLocker locker(&mutex);
    viewportWidth = size.width();
    viewportHeight = size.height();
}

void GLPainter::paintFps(QPainter *painter)
{
	// calculate fps
	glWidget->m_fpsCounter++;
	if (glWidget->m_fpsCounterTime.elapsed() >= 1000) {
		glWidget->m_fpsCount = glWidget->m_fpsCounter;
		glWidget->m_fpsCounter = 0;
		glWidget->m_fpsCounterTime.restart();
	}
	// set font and draw fps
	QFont font = painter->font();
	font.setPointSize(12);
	painter->setFont(font);
	painter->setPen(Qt::red);
	painter->drawText(QRectF(80.0f, 0.0f, 100.0f, 60.0f),
					  Qt::AlignCenter,
					  QString("%1 FPS").arg(glWidget->m_fpsCount));
}

void GLPainter::paintFrame(QImage frame) {
    QMutexLocker locker(&mutex);

    glWidget->makeCurrent();
    QPainter painter(glWidget);
//	painter.begin(this);

	// clear screen early only when we are not drawing on entire screen later
	if (glWidget->m_keepAspectRatio)
		painter.fillRect(glWidget->rect(), Qt::black);

    GLuint t = glWidget->bindTexture(frame);
    glWidget->drawTexture(glWidget->m_dstRect, t);
    glWidget->deleteTexture(t);

	if (glWidget->m_fpsVisible)
		paintFps(&painter);

	// draw buttons
	glWidget->m_hostInput->paint(&painter);
	painter.end();
}

