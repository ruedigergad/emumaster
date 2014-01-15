/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AMIGAEVENTS_H
#define AMIGAEVENTS_H

#include "amiga.h"

/* Every Amiga hardware clock cycle takes this many "virtual" cycles.  This
   used to be hardcoded as 1, but using higher values allows us to time some
   stuff more precisely.
   512 is the official value from now on - it can't change, unless we want
   _another_ config option "finegrain2_m68k_speed". */
#define CYCLE_UNIT 512

class AmigaEvent {
public:
	int active;
	u32 time;
};

enum AmigaEventType {
	AmigaEventHSync,
	AmigaEventCopper,
	AmigaEventSpu,
	AmigaEventCia,
	AmigaEventBlitter,
	AmigaEventDisk,
	AmigaEventMax
};

extern AmigaEvent amigaEvents[AmigaEventMax];
extern u32 amigaCycles;
extern u32 amigaNextEvent;
extern bool amigaEventVSync;

extern void amigaEventsInit();

extern void amigaHSyncHandler();
extern void amigaCopperHandler();
extern void amigaSpuHandler();
extern void amigaCiaHandler();
extern void amigaBlitterHandler();
extern void amigaDiskHandler();

static __inline__ void amigaEventsSchedule() {
	u32 minTime = ~0L;
	for (int i = 0; i < AmigaEventMax; i++) {
		if (amigaEvents[i].active) {
			u32 eventTime = amigaEvents[i].time - amigaCycles;
			if (eventTime < minTime)
				minTime = eventTime;
		}
	}
	amigaNextEvent = amigaCycles + minTime;
}

static __inline__ void amigaEventsHandle(u32 cycles) {
	while ((amigaNextEvent - amigaCycles) <= cycles) {
		cycles -= (amigaNextEvent - amigaCycles);
		amigaCycles = amigaNextEvent;

		if (amigaEvents[AmigaEventHSync].active && amigaEvents[AmigaEventHSync].time == amigaCycles)
			amigaHSyncHandler();
		if (amigaEvents[AmigaEventCopper].active && amigaEvents[AmigaEventCopper].time == amigaCycles)
			amigaCopperHandler();
		if (amigaEvents[AmigaEventSpu].active && amigaEvents[AmigaEventSpu].time == amigaCycles)
			amigaSpuHandler();
		if (amigaEvents[AmigaEventCia].active && amigaEvents[AmigaEventCia].time == amigaCycles)
			amigaCiaHandler();
		if (amigaEvents[AmigaEventBlitter].active && amigaEvents[AmigaEventBlitter].time == amigaCycles)
			amigaBlitterHandler();
		if (amigaEvents[AmigaEventDisk].active && amigaEvents[AmigaEventDisk].time == amigaCycles)
			amigaDiskHandler();

		amigaEventsSchedule();
	}
	amigaCycles += cycles;

	if (amigaEventVSync) {
		amigaEmu.vSync();
		amigaEventVSync = false;
	}
}

#endif // AMIGAEVENTS_H
