/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	(c) Copyright 2011, elemental
*/

#include "pico.h"
#include "ym2612.h"
#include "sn76496.h"
#include "mp3player.h"

static void cycloneSl(const QString &name, Cyclone *cyclone) {
	u8 data[0x60];
	memset(&data, 0, sizeof(data));

	if (emsl.save)
		CyclonePack(cyclone, data);

	emsl.array(name, data, sizeof(data));

	if (!emsl.save)
		CycloneUnpack(cyclone, data);
}

static void picoCdSl() {
	if (emsl.save) {
		if (picoEmu.mp3Player())
			Pico_mcd->m.audio_offset = picoEmu.mp3Player()->pos();
		if (Pico_mcd->s68k_regs[3]&4) // 1M mode?
			wram_1M_to_2M(Pico_mcd->word_ram2M);
	}
	emsl.begin("pico.mcd");
	cycloneSl("cpu.s68k", &PicoCpuCS68k);
	emsl.array("prg_ram", Pico_mcd->prg_ram, sizeof(Pico_mcd->prg_ram));
	emsl.array("word_ram", Pico_mcd->word_ram2M, sizeof(Pico_mcd->word_ram2M)); // in 2M format
	emsl.array("pcm_ram", Pico_mcd->pcm_ram, sizeof(Pico_mcd->pcm_ram));
	emsl.array("bram", Pico_mcd->bram, sizeof(Pico_mcd->bram));
	emsl.array("ga_regs", Pico_mcd->s68k_regs, sizeof(Pico_mcd->s68k_regs)); // GA regs, not CPU regs
	emsl.array("pcm", &Pico_mcd->pcm, sizeof(Pico_mcd->pcm));
	emsl.array("cdd", &Pico_mcd->cdd, sizeof(Pico_mcd->cdd));
	emsl.array("cdc", &Pico_mcd->cdc, sizeof(Pico_mcd->cdc));
	emsl.array("scd", &Pico_mcd->scd, sizeof(Pico_mcd->scd));
	emsl.array("rc", &Pico_mcd->rot_comp, sizeof(Pico_mcd->rot_comp));
	emsl.array("misc_cd", &Pico_mcd->m, sizeof(Pico_mcd->m));
	emsl.end();

	if (Pico_mcd->s68k_regs[3]&4) // 1M mode?
		wram_2M_to_1M(Pico_mcd->word_ram2M);
	if (!emsl.save) {
		PicoMemResetCD(Pico_mcd->s68k_regs[3]);
#ifdef _ASM_CD_MEMORY_C
		if (Pico_mcd->s68k_regs[3]&4)
			PicoMemResetCDdecode(Pico_mcd->s68k_regs[3]);
#endif
		if (picoEmu.mp3Player()) {
			if (Pico_mcd->m.audio_track > 0 && Pico_mcd->m.audio_track < Pico_mcd->TOC.Last_Track)
				Pico_mcd->TOC.playTrack(Pico_mcd->m.audio_track, Pico_mcd->m.audio_offset);
		}
	}
}

void PicoEmu::sl() {
	if ((PicoMCD & 1) && emsl.save)
		Pico_mcd->m.hint_vector = *(u16 *)(Pico_mcd->bios + 0x72);

	emsl.begin("pico");

	// memory areas
	emsl.array("ram", Pico.ram, sizeof(Pico.ram));
	emsl.array("vram", Pico.vram, sizeof(Pico.vram));
	emsl.array("zram", Pico.zram, sizeof(Pico.zram));
	emsl.array("cram", Pico.cram, sizeof(Pico.cram));
	emsl.array("vsram", Pico.vsram, sizeof(Pico.vsram));

	// cpus
	cycloneSl("cpu.m68k", &PicoCpuCM68k);
	z80Sl("cpu.z80");

	emsl.array("misc", &Pico.m, sizeof(Pico.m));
	emsl.array("video", &Pico.video, sizeof(Pico.video));

	sn76496.sl("psg");
	ym2612.sl("fm");

	if (!emsl.save)
		Pico.m.dirtyPal = 1;

	emsl.end();

	if (PicoMCD & 1) {
		picoCdSl();
		if (!emsl.save)
			*(u16 *)(Pico_mcd->bios + 0x72) = Pico_mcd->m.hint_vector;
	}

	/* TODO save sram
	FILE *sramFile;
	int sram_size;
	unsigned char *sram_data;
	int truncate = 1;
	if (PicoMCD&1) {
		if (PicoOpt&0x8000) { // MCD RAM cart?
			sram_size = 0x12000;
			sram_data = SRam.data;
			if (sram_data)
				memcpy32((int *)sram_data, (int *)Pico_mcd->bram, 0x2000/4);
		} else {
			sram_size = 0x2000;
			sram_data = Pico_mcd->bram;
			truncate  = 0; // the .brm may contain RAM cart data after normal brm
		}
	} else {
		sram_size = SRam.end-SRam.start+1;
		if(Pico.m.sram_reg & 4) sram_size=0x2000;
		sram_data = SRam.data;
	}
	if (!sram_data) return 0; // SRam forcefully disabled for this game

	if (load) {
		sramFile = fopen(saveFname, "rb");
		if(!sramFile) return -1;
		fread(sram_data, 1, sram_size, sramFile);
		fclose(sramFile);
		if ((PicoMCD&1) && (PicoOpt&0x8000))
			memcpy32((int *)Pico_mcd->bram, (int *)sram_data, 0x2000/4);
	} else {
		// sram save needs some special processing
		// see if we have anything to save
		for (; sram_size > 0; sram_size--)
			if (sram_data[sram_size-1]) break;

		if (sram_size) {
			sramFile = fopen(saveFname, truncate ? "wb" : "r+b");
			if (!sramFile) sramFile = fopen(saveFname, "wb"); // retry
			if (!sramFile) return -1;
			ret = fwrite(sram_data, 1, sram_size, sramFile);
			ret = (ret != sram_size) ? -1 : 0;
			fclose(sramFile);
		}
	}*/
}
