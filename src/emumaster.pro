#
# Build notes for the SailfishOS SDK:
#
# Add SSH key to SDK authorized_keys (Needs to be done only once.):
#   vim mersdk/ssh/root/authorized_keys
#
# Log-in to the BuildVM (Make sure that the SDK is running in the Qt Creator IDE.):
#   ssh mersdk@127.0.0.1 -p 2222
#
# cd /home/src1/SailfishOS/workspace/emumaster/src
#   (Your path may differ.)
# sb2 -t SailfishOS-armv7hl qmake -r emumaster.pro
# sb2 -t SailfishOS-armv7hl make
#


TEMPLATE = subdirs

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
