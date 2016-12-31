TEMPLATE = subdirs

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

SUBDIRS += \
    base \
    snes \
    diskgallery

#	gba snes pico \
# nes gba snes psx amiga pico \

OTHER_FILES += \
    ../todo.txt \
    ../include/numeric.h \
    qtc_packaging/debian_harmattan/rules \
    qtc_packaging/debian_harmattan/copyright \
    qtc_packaging/debian_harmattan/control \
    qtc_packaging/debian_harmattan/compat \
    qtc_packaging/debian_harmattan/changelog \
    ../wiki.txt \
    qtc_packaging/debian_fremantle/rules \
    qtc_packaging/debian_fremantle/README \
    qtc_packaging/debian_fremantle/copyright \
    qtc_packaging/debian_fremantle/control \
    qtc_packaging/debian_fremantle/compat \
    qtc_packaging/debian_fremantle/changelog
