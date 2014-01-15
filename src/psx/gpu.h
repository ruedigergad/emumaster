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

#ifndef PSXGPU_H
#define PSXGPU_H

#include "common.h"

typedef void (* GPUwriteStatus)(u32);
typedef void (* GPUwriteData)(u32);
typedef void (* GPUwriteDataMem)(u32 *, int);
typedef u32 (* GPUreadStatus)();
typedef u32 (* GPUreadData)();
typedef void (* GPUreadDataMem)(u32 *, int);
typedef u32 (* GPUdmaChain)(u32 *,u32);
typedef void (* GPUupdateLace)();

#if defined(__cplusplus)

class PsxGpu {
public:
	virtual bool init() = 0;
	virtual void shutdown();
	virtual const QImage &frame() = 0;
	virtual void setDrawEnabled(bool drawEnabled) = 0;

	virtual void sl() = 0;
};

extern PsxGpu *psxGpu;

extern "C" {
#endif

extern GPUreadStatus    GPU_readStatus;
extern GPUreadData      GPU_readData;
extern GPUreadDataMem   GPU_readDataMem;
extern GPUwriteStatus   GPU_writeStatus;
extern GPUwriteData     GPU_writeData;
extern GPUwriteDataMem  GPU_writeDataMem;
extern GPUdmaChain      GPU_dmaChain;
extern GPUupdateLace    GPU_updateLace;

#if defined(__cplusplus)
}
#endif

#endif // PSXGPU_H
