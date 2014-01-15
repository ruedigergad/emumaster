include(../emu.pri)

DEFINES += USE_CYCLONE_MEMHANDLERS

unix {
	qml.path = /opt/emumaster/qml/$${TARGET}
	qml.files = \
		../../qml/$${TARGET}/main.qml \
		../../qml/$${TARGET}/MainPage.qml \
		../../qml/$${TARGET}/SettingsPage.qml

	gameclassify.path = /usr/share/policy/etc/syspart.conf.d
	gameclassify.files += $${TARGET}.conf

	INSTALLS += qml gameclassify
}

HEADERS += \
    zfile.h \
    keybuf.h \
    joystick.h \
    events.h \
    drawing.h \
    disk.h \
    custom.h \
    cia.h \
    blitter.h \
    blitfunc.h \
    blit.h \
    cyclone.h \
    spu.h \
    cpu.h \
    mem.h \
    amiga.h

SOURCES += \
	blitfunc.cpp \
	blittable.cpp \
	blitter.cpp \
	cia.cpp \
	custom.cpp \
	disk.cpp \
	drawing.cpp \
    cyclone.S \
	spu.cpp \
	mem.cpp \
	mem_handlers.S \
	cpu.cpp \
    keyb.cpp \
    amiga.cpp
