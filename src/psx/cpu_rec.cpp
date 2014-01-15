/*
 * (C) Gražvydas "notaz" Ignotas, 2010-2011
 *
 * This work is licensed under the terms of GNU GPL version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "cpu_rec.h"
#include "hle.h"
#include "cdrom.h"
#include "dma.h"
#include "mdec.h"
#include "gte_neon.h"
#include "sio.h"
#include "counters.h"
#include "new_dynarec/emu_if.h"
#include "new_dynarec/pcsxmem.h"

PsxCpuRec psxRec;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

//#define evprintf printf
#define evprintf(...)

char invalid_code[0x100000];
u32 event_cycles[PSXINT_COUNT];

static void schedule_timeslice(void)
{
	u32 i, c = psxRegs.cycle;
	s32 min, dif;

	min = psxNextsCounter + psxNextCounter - c;
	for (i = 0; i < ARRAY_SIZE(event_cycles); i++) {
		dif = event_cycles[i] - c;
		//evprintf("  ev %d\n", dif);
		if (0 < dif && dif < min)
			min = dif;
	}
	next_interupt = c + min;
}

typedef void (irq_func)();

static irq_func * const irq_funcs[] = {
	sioInterrupt,
	cdrInterrupt,
	cdrReadInterrupt,
	gpuInterrupt,
	mdec1Interrupt,
	spuInterrupt,
	0,
	mdec0Interrupt,
	gpuotcInterrupt,
	cdrDmaInterrupt,
	0,
	0,
	cdrLidSeekInterrupt,
	cdrPlayInterrupt,
	0
};

/* local dupe of psxCpuBranchTest, using event_cycles */
static void irq_test(void)
{
	u32 irqs = psxRegs.interrupt;
	u32 cycle = psxRegs.cycle;
	u32 irq, irq_bits;

	if ((psxRegs.cycle - psxNextsCounter) >= psxNextCounter)
		psxRcntUpdate();

	// irq_funcs() may queue more irqs
	psxRegs.interrupt = 0;

	for (irq = 0, irq_bits = irqs; irq_bits != 0; irq++, irq_bits >>= 1) {
		if (!(irq_bits & 1))
			continue;
		if ((s32)(cycle - event_cycles[irq]) >= 0) {
			irqs &= ~(1 << irq);
			irq_funcs[irq]();
		}
	}
	psxRegs.interrupt |= irqs;

	if ((psxHu32(0x1070) & psxHu32(0x1074)) && (Status & 0x401) == 0x401) {
		psxCpuException(0x400, 0);
		pending_exception = 1;
	}
}

void gen_interupt() {
	evprintf("  +ge %08x, %u->%u\n", psxRegs.pc, psxRegs.cycle, next_interupt);

	irq_test();
	//psxCpuBranchTest();
	//pending_exception = 1;

	schedule_timeslice();

	evprintf("  -ge %08x, %u->%u (%d)\n", psxRegs.pc, psxRegs.cycle,
		next_interupt, next_interupt - psxRegs.cycle);
}

// from interpreter
extern void MTC0(int reg, u32 val);

void pcsx_mtc0(u32 reg)
{
	evprintf("MTC0 %d #%x @%08x %u\n", reg, readmem_word, psxRegs.pc, psxRegs.cycle);
	MTC0(reg, readmem_word);
	gen_interupt();
}

void pcsx_mtc0_ds(u32 reg)
{
	evprintf("MTC0 %d #%x @%08x %u\n", reg, readmem_word, psxRegs.pc, psxRegs.cycle);
	MTC0(reg, readmem_word);
}

void new_dyna_save(void)
{
	// psxRegs.intCycle is always maintained, no need to convert
}

void new_dyna_restore(void)
{
	int i;
	for (i = 0; i < PSXINT_COUNT; i++)
		event_cycles[i] = psxRegs.intCycle[i].sCycle + psxRegs.intCycle[i].cycle;
}

void *gte_handlers[64];

/* from gte.txt.. not sure if this is any good. */
const char gte_cycletab[64] = {
	/*   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f */
	 0, 15,  0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  6,  0,  0,  0,
	 8,  8,  8, 19, 13,  0, 44,  0,  0,  0,  0, 17, 11,  0, 14,  0,
	30,  0,  0,  0,  0,  0,  0,  0,  5,  8, 17,  0,  0,  5,  6,  0,
	23,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  5,  5, 39,
};

// execute until predefined leave points
// (HLE softcall exit and BIOS fastboot end)
static void cpuRecExecuteBlock() {
	schedule_timeslice();

	evprintf("ari64_execute %08x, %u->%u (%d)\n", psxRegs.pc,
		psxRegs.cycle, next_interupt, next_interupt - psxRegs.cycle);

	new_dyna_start();

	evprintf("ari64_execute end %08x, %u->%u (%d)\n", psxRegs.pc,
		psxRegs.cycle, next_interupt, next_interupt - psxRegs.cycle);
}

static void cpuRecExecute() {
	while (!stop) {
		cpuRecExecuteBlock();
		evprintf("drc left @%08x\n", psxRegs.pc);
	}
}

static void cpuRecClear(u32 addr, u32 size) {
	u32 start, end, main_ram;

	size *= 4; /* PCSX uses DMA units */

	evprintf("ari64_clear %08x %04x\n", addr, size);

	/* check for RAM mirrors */
	main_ram = (addr & 0xffe00000) == 0x80000000;

	start = addr >> 12;
	end = (addr + size) >> 12;

	for (; start <= end; start++)
		if (!main_ram || !invalid_code[start])
			invalidate_block(start);
}

bool PsxCpuRec::init() {
	stop = 0;

	execute			= cpuRecExecute;
	executeBlock	= cpuRecExecuteBlock;
	clear			= cpuRecClear;

	extern void (*psxCP2[64])();
	extern void psxNULL();
	size_t i;

	new_dynarec_init();
	new_dyna_pcsx_mem_init();

	for (i = 0; i < ARRAY_SIZE(gte_handlers); i++)
		if (psxCP2[i] != psxNULL)
			gte_handlers[i] = (void *)psxCP2[i];
#if !defined(ARMv5_ONLY)
	gte_handlers[0x01] = (void *)gteRTPS_neon;
	gte_handlers[0x30] = (void *)gteRTPT_neon;
	gte_handlers[0x12] = (void *)gteMVMVA_neon;
	gte_handlers[0x06] = (void *)gteNCLIP_neon;
#endif
	psxH_ptr = psxH;

	return true;
}

void PsxCpuRec::shutdown() {
	new_dynarec_cleanup();
}

void PsxCpuRec::reset() {
	printf("ari64_reset\n");
	new_dyna_pcsx_mem_reset();
	invalidate_all_pages();
	new_dyna_restore();
	pending_exception = 1;
}
