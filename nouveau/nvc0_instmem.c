/*
 * Copyright 2010 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */

#include "drmP.h"

#include "nouveau_drv.h"

int
nvc0_instmem_populate(struct drm_device *dev, struct nouveau_gpuobj *gpuobj,
		      uint32_t *size)
{
	return 0;
}

void
nvc0_instmem_clear(struct drm_device *dev, struct nouveau_gpuobj *gpuobj)
{
}

int
nvc0_instmem_bind(struct drm_device *dev, struct nouveau_gpuobj *gpuobj)
{
	return 0;
}

int
nvc0_instmem_unbind(struct drm_device *dev, struct nouveau_gpuobj *gpuobj)
{
	return 0;
}

void
nvc0_instmem_flush(struct drm_device *dev)
{
}

int
nvc0_instmem_suspend(struct drm_device *dev)
{
	return 0;
}

void
nvc0_instmem_resume(struct drm_device *dev)
{
}

int
nvc0_instmem_init(struct drm_device *dev)
{
	return 0;
}

void
nvc0_instmem_takedown(struct drm_device *dev)
{
}

