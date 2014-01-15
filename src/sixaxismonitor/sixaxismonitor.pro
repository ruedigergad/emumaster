DESTDIR = ../../bin
INCLUDEPATH += ..
LIBS += -L../../lib -lbase
QT += opengl declarative network

unix {
	QMAKE_LFLAGS += -Wl,--rpath-link,../../lib -Wl,--rpath,/opt/emumaster/lib
	target.path = /opt/emumaster/bin
	qml.path = /opt/emumaster/qml/sixaxismonitor
	qml.files = \
		../../qml/sixaxismonitor/main.qml \
		../../qml/sixaxismonitor/MainPage.qml
	INSTALLS += target qml
}

SOURCES += \
    sixaxismonitor.cpp \
    sixaxisserver.cpp \
    sixaxisdevice.cpp

HEADERS += \
    sixaxismonitor.h \
    sixaxisserver.h \
    sixaxisdevice.h

