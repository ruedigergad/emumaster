include(../emu.pri)

DEFINES += EMU_C68K _USE_DRZ80 _ASM_DRAW_C _ASM_MEMORY_C _ASM_YM2612_C _ASM_MISC_C _ASM_CD_PICO_C _ASM_CD_MEMORY_C
CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 libxml-2.0 gstreamer-0.10

HEADERS += \
    pico.h \
    ym2612.h \
    sn76496.h \
    drz80.h \
    port_config.h \
    pico_frame_hints.h \
    memcmn.h \
    lc89510.h \
    cd_sys.h \
    cd_pcm.h \
    cd_gfx.h \
    cd_file.h \
    cd_cell_map.h \
    cheat.h \
    cart.h \
    cyclone.h \
    mp3player.h

ASM_SOURCES += \
    sound_asm.S \
    ym2612_asm.S \
    misc_asm.S \
    mem_asm.S \
    draw2_asm.S \
    draw_asm.S \
    port_config.S \
    drz80.S \
    cyclone.S \
    cd_pico_asm.S \
    cd_misc_asm.S \
    cd_mem_asm.S

SOURCES += \
    ym2612.cpp \
    sn76496.cpp \
    pico.cpp \
    video_port.cpp \
    sek.cpp \
    draw2.cpp \
    draw.cpp \
    misc.cpp \
    mem.cpp \
    sound.cpp \
    lc89510.cpp \
    cd_sys.cpp \
    cd_sek.cpp \
    cd_pico.cpp \
    cd_pcm.cpp \
    cd_misc.cpp \
    cd_mem.cpp \
    cd_gfx.cpp \
    cheat.cpp \
    cart.cpp \
    $$ASM_SOURCES \
    state.cpp \
    pad.cpp \
    sound_z80.cpp \
    cd_file.cpp \
    mp3player.cpp
