DESTDIR = ../../bin
LIBS += -L../../lib -lbase
INCLUDEPATH += ..

#INCLUDEPATH += /usr/include/sailfishapp
#CONFIG += link_pkgconfig
#PKGCONFIG += sailfishapp

QT += opengl qml quick

#contains(CONFIG,release) {
#	QMAKE_CFLAGS_RELEASE -= -O2
#	QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO -= -O2
#	QMAKE_CXXFLAGS += \
#		-O3 \
#		-march=armv7-a \
#		-mcpu=cortex-a8 \
#		-mtune=cortex-a8 \
#		-mfpu=neon \
#		-ffast-math \
#		-ftemplate-depth-36 \
#		-fstrict-aliasing \
#		-mstructure-size-boundary=32 \
#		-falign-functions=32
#}

unix {
	MEEGO_VERSION_MAJOR     = 1
	MEEGO_VERSION_MINOR     = 2
	MEEGO_VERSION_PATCH     = 0
	MEEGO_EDITION           = harmattan
	DEFINES += MEEGO_EDITION_HARMATTAN
}

QMAKE_CFLAGS_RELEASE = -O2\
    -g\
    -pipe\
    -Wall\
    -Wp,-D_FORTIFY_SOURCE=2\
    -fexceptions\
    -fstack-protector\
    --param=ssp-buffer-size=4\
    -Wformat\
    -Wformat-security\
    -fmessage-length=0\
    -march=armv7-a\
    -mfloat-abi=hard\
    -mfpu=vfpv3-d16\
    -marm\
    -mno-thumb-interwork\
    -Wno-psabi
QMAKE_CFLAGS_DEBUG = -O2\
    -g\
    -pipe\
    -Wall\
    -Wp,-D_FORTIFY_SOURCE=2\
    -fexceptions\
    -fstack-protector\
    --param=ssp-buffer-size=4\
    -Wformat\
    -Wformat-security\
    -fmessage-length=0\
    -march=armv7-a\
    -mfloat-abi=hard\
    -mfpu=vfpv3-d16\
    -marm\
    -mno-thumb-interwork\
    -Wno-psabi
QMAKE_CXXFLAGS_RELEASE = -O2\
    -g\
    -pipe\
    -Wall\
    -Wp,-D_FORTIFY_SOURCE=2\
    -fexceptions\
    -fstack-protector\
    --param=ssp-buffer-size=4\
    -Wformat\
    -Wformat-security\
    -fmessage-length=0\
    -march=armv7-a\
    -mfloat-abi=hard\
    -mfpu=vfpv3-d16\
    -marm\
    -mno-thumb-interwork\
    -Wno-psabi
QMAKE_CXXFLAGS_DEBUG = -O2\
    -g\
    -pipe\
    -Wall\
    -Wp,-D_FORTIFY_SOURCE=2\
    -fexceptions\
    -fstack-protector\
    --param=ssp-buffer-size=4\
    -Wformat\
    -Wformat-security\
    -fmessage-length=0\
    -march=armv7-a\
    -mfloat-abi=hard\
    -mfpu=vfpv3-d16\
    -marm\
    -mno-thumb-interwork\
    -Wno-psabi

unix {
	QMAKE_LFLAGS += -Wl,--rpath-link,../../lib -Wl,--rpath,/opt/emumaster/bin -Wl,--rpath,/opt/emumaster/lib
	target.path = /opt/emumaster/bin

	gameclassify.path = /usr/share/policy/etc/syspart.conf.d
	gameclassify.files += $${TARGET}.conf

	INSTALLS += target gameclassify
}
