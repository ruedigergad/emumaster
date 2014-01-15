/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "diskimageprovider.h"
#include <base/pathmanager.h>
#include <QImage>
#include <QPainter>
#include <QFile>

DiskImageProvider::DiskImageProvider() :
	QQuickImageProvider(Image),
	m_noScreenShot(256, 256, QImage::Format_RGB32)
{
	QPainter painter;
	painter.begin(&m_noScreenShot);
	painter.fillRect(QRectF(QPointF(), m_noScreenShot.size()), QColor(qRgb(0x4B, 0x4A, 0x4C)));
	painter.setPen(qRgb(0xE0, 0xE1, 0xE2));
	painter.translate(180, -60);
	painter.rotate(60);
	QFont font = painter.font();
	font.setBold(true);
	font.setPixelSize(35);
	painter.setFont(font);
	QTextOption option;
	option.setAlignment(Qt::AlignCenter);
	painter.drawText(m_noScreenShot.rect(), QObject::tr("NO SCREENSHOT"), option);
	painter.end();
}

QImage DiskImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
	Q_UNUSED(size)
	Q_UNUSED(requestedSize)
	QString idGoodPart = id.left(id.indexOf('*'));
	QString path = QString("%1/screenshot/%2.jpg")
			.arg(pathManager.userDataDirPath())
			.arg(idGoodPart);
	if (!QFile::exists(path))
		return m_noScreenShot;
	QImage img;
	if (!img.load(path))
		return m_noScreenShot;
	return img;
}
