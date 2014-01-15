include(../emu.pri)

LIBS += -lz

HEADERS += \
    ppf.h \
    misc.h \
    mdec.h \
    gte_neon.h \
    gte_divider.h \
    gte.h \
    decode_xa.h \
    debug.h \
    coff.h \
    cheat.h \
    cdrom.h \
    cdriso.h \
    spu.h \
    sjisfont.h \
    sio.h \
    cpu.h \
	pad.h \
    new_dynarec/pcsxmem.h \
    new_dynarec/new_dynarec.h \
    new_dynarec/fpu.h \
    new_dynarec/emu_if.h \
    new_dynarec/assem_arm.h \
	gpu_unai_raster_sprite.h \
	gpu_unai_raster_polygon.h \
	gpu_unai_raster_line.h \
	gpu_unai_raster_image.h \
	gpu_unai_inner_light.h \
	gpu_unai_inner_blend.h \
	gpu_unai_inner.h \
	gpu_unai_fixedpoint.h \
	gpu_unai_command.h \
	gpu_unai.h \
    gpu.h \
    spu_registers.h \
    spu_null.h \
    cpu_int.h \
    cpu_rec.h \
    bios.h \
    common.h \
    counters.h \
    dma.h \
    hle.h \
    hw.h \
    mem.h \
    socket.h \
    spu_fran.h \
    psx.h

SOURCES += \
	gte_neon.S \
    cdriso.cpp \
    cdrom.cpp \
    cheat.cpp \
    debug.cpp \
    decode_xa.cpp \
    disr3000a.cpp \
    gte.cpp \
    mdec.cpp \
    misc.cpp \
    ppf.cpp \
    sio.cpp \
	pad.cpp \
    new_dynarec/fpu.c \
    new_dynarec/new_dynarec.c \
	new_dynarec/pcsxmem.c \
	new_dynarec/linkage_arm.S \
	gpu.cpp \
	gpu_unai_blit.S \
	gpu_unai.cpp \
    spu.cpp \
    spu_null.cpp \
    cpu.cpp \
    cpu_int.cpp \
    cpu_rec.cpp \
    bios.cpp \
    counters.cpp \
    dma.cpp \
    hle.cpp \
    hw.cpp \
    mem.cpp \
    socket.cpp \
    spu_fran.cpp \
    psx.cpp

OTHER_FILES += \
	new_dynarec/assem_arm.c \
	new_dynarec/pcsxmem_inline.c
