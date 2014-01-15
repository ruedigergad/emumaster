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

#include <QtGui/QApplication>
#include <QDeclarativeContext>
#include <qdeclarative.h>
#include "qmlapplicationviewer.h"
#include "debugger.h"

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QScopedPointer<QApplication> app(createApplication(argc, argv));

	qmlRegisterType<NesDebugger>();
	qmlRegisterType<NesProfiler>();
	NesDebugger *dbg = new NesDebugger();

    QmlApplicationViewer viewer;
	viewer.rootContext()->setContextProperty("dbg", dbg);
	viewer.setOrientation(QmlApplicationViewer::ScreenOrientationAuto);
    viewer.setMainQmlFile(QLatin1String("qml/nesdebugger/main.qml"));
	viewer.showExpanded();

    return app->exec();
}
