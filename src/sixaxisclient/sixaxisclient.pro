TEMPLATE = lib
DESTDIR = ../../lib
INCLUDEPATH += ..
QT += network

unix {
	QMAKE_LFLAGS += -Wl,--rpath,/opt/emumaster/lib
	target.path = /opt/emumaster/lib
	INSTALLS += target
}

HEADERS += \
    sixaxis.h

SOURCES += \
    sixaxis.cpp
