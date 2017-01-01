#include <QDebug>
#include <QRect>
#include "emuview.h"
#include "frameitem.h"
#include "hostinput.h"
#include "touchinputdevice.h"

FrameItem::FrameItem(QQuickItem *parent) :
    QQuickPaintedItem(parent),
    m_currentFrame(0)
{
    m_fpsCount = 0;
    m_fpsCounter = 0;
    m_fpsCounterTime.start();

    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

void FrameItem::handleNewFrame(QImage frame) {
    m_currentFrame = frame;

    update();
}

void FrameItem::paint(QPainter *painter)
{
    if (! m_currentFrame.isNull()) {
        painter->drawImage(contentsBoundingRect(), m_currentFrame);
        m_emuView->hostInput()->paint(painter);
        paintFps(painter);
    }
}

void FrameItem::paintFps(QPainter *painter)
{
    // calculate fps
    m_fpsCounter++;
    if (m_fpsCounterTime.elapsed() >= 1000) {
        m_fpsCount = m_fpsCounter;
        m_fpsCounter = 0;
        m_fpsCounterTime.restart();
    }

    // set font and draw fps
    QFont font = painter->font();
    font.setPointSize(32);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::red);
    painter->drawText(QRectF(15.0f, 5.0f, 140.0f, 80.0f),
                      Qt::AlignCenter,
                      QString("%1 FPS").arg(m_fpsCount));
}

void FrameItem::setEmuView(QObject *emuView) {
    qDebug("Setting emuView.");

    m_emuView = qobject_cast<EmuView *>(emuView);
    connect(m_emuView, SIGNAL(videoFrameChanged(QImage)), this, SLOT(handleNewFrame(QImage)));

    qDebug() << "FrameItem Bounding Rectangle: " << contentsBoundingRect().width() << "x" << contentsBoundingRect().height();

    m_emuView->hostInput()->touchInputDevice()->setHeight((int) contentsBoundingRect().height());
    m_emuView->hostInput()->touchInputDevice()->setWidth((int) contentsBoundingRect().width());
}

void FrameItem::touchEvent(QTouchEvent *touchEvent) {
    m_emuView->hostInput()->processTouch(touchEvent);
}

