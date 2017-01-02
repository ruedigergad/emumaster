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
        node->setRect(boundingRect());
    }

    if (! m_currentFrame.isNull()) {
        QSGTexture *oldTexture = node->texture();

        QSGTexture *texture = window()->createTextureFromImage(m_currentFrame);
        node->setTexture(texture);

        if (oldTexture)
        {
            delete oldTexture;
        }
    }

    updateFps();

    return node;
}

void FrameItem::updateFps()
{
    m_fpsCounter++;
    if (m_fpsCounterTime.elapsed() >= 1000) {
        m_fpsCount = m_fpsCounter;
        m_fpsCounter = 0;
        m_fpsCounterTime.restart();

        fpsUpdated(m_fpsCount);
    }
}

void FrameItem::setEmuView(QObject *emuView) {
    qDebug("Setting emuView.");

    m_emuView = qobject_cast<EmuView *>(emuView);
    connect(m_emuView, SIGNAL(videoFrameChanged(QImage)), this, SLOT(handleNewFrame(QImage)));

    qDebug() << "FrameItem Bounding Rectangle: " << boundingRect().width() << "x" << boundingRect().height();
}
