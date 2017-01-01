#include <QDebug>
#include <QRect>
#include <QSGSimpleTextureNode>
#include "emuview.h"
#include "frameitem.h"
#include "hostinput.h"
#include "touchinputdevice.h"

FrameItem::FrameItem(QQuickItem *parent) :
    QQuickItem(parent),
    m_currentFrame(0)
{
    m_fpsCount = 0;
    m_fpsCounter = 0;
    m_fpsCounterTime.start();
}

void FrameItem::handleNewFrame(QImage frame) {
    m_currentFrame = frame;
    setFlag(QQuickItem::ItemHasContents, true);
    update();
}

QSGNode* FrameItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
    }

    if (! m_currentFrame.isNull()) {
        QSGTexture *texture = window()->createTextureFromImage(m_currentFrame);
        node->setTexture(texture);
    }

    node->setRect(boundingRect());
    printFps();

    return node;
}

void FrameItem::printFps()
{
    // calculate fps
    m_fpsCounter++;
    if (m_fpsCounterTime.elapsed() >= 1000) {
        m_fpsCount = m_fpsCounter;
        m_fpsCounter = 0;
        m_fpsCounterTime.restart();

        qDebug() << "FPS:" << m_fpsCount;
    }
}

void FrameItem::setEmuView(QObject *emuView) {
    qDebug("Setting emuView.");

    m_emuView = qobject_cast<EmuView *>(emuView);
    connect(m_emuView, SIGNAL(videoFrameChanged(QImage)), this, SLOT(handleNewFrame(QImage)));

    qDebug() << "FrameItem Bounding Rectangle: " << boundingRect().width() << "x" << boundingRect().height();

//    m_emuView->hostInput()->touchInputDevice()->setHeight((int) contentsBoundingRect().height());
//    m_emuView->hostInput()->touchInputDevice()->setWidth((int) contentsBoundingRect().width());
}

void FrameItem::touchEvent(QTouchEvent *touchEvent) {
    m_emuView->hostInput()->processTouch(touchEvent);
}

