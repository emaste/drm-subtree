/*
 * Copyright 2012 Red Hat Inc.
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
#include "nouveau_dma.h"
#include <engine/fifo.h>
#include <core/ramht.h>
#include "nouveau_fence.h"
#include "nv50_display.h"

struct nv84_fence_chan {
	struct nouveau_fence_chan base;
};

struct nv84_fence_priv {
	struct nouveau_fence_priv base;
	struct nouveau_gpuobj *mem;
};

static int
nv84_fence_emit(struct nouveau_fence *fence)
{
	struct nouveau_channel *chan = fence->channel;
	int ret = RING_SPACE(chan, 7);
	if (ret == 0) {
		BEGIN_NV04(chan, 0, NV11_SUBCHAN_DMA_SEMAPHORE, 1);
		OUT_RING  (chan, NvSema);
		BEGIN_NV04(chan, 0, NV84_SUBCHAN_SEMAPHORE_ADDRESS_HIGH, 4);
		OUT_RING  (chan, upper_32_bits(chan->id * 16));
		OUT_RING  (chan, lower_32_bits(chan->id * 16));
		OUT_RING  (chan, fence->sequence);
		OUT_RING  (chan, NV84_SUBCHAN_SEMAPHORE_TRIGGER_WRITE_LONG);
		FIRE_RING (chan);
	}
	return ret;
}


static int
nv84_fence_sync(struct nouveau_fence *fence,
		struct nouveau_channel *prev, struct nouveau_channel *chan)
{
	int ret = RING_SPACE(chan, 7);
	if (ret == 0) {
		BEGIN_NV04(chan, 0, NV11_SUBCHAN_DMA_SEMAPHORE, 1);
		OUT_RING  (chan, NvSema);
		BEGIN_NV04(chan, 0, NV84_SUBCHAN_SEMAPHORE_ADDRESS_HIGH, 4);
		OUT_RING  (chan, upper_32_bits(prev->id * 16));
		OUT_RING  (chan, lower_32_bits(prev->id * 16));
		OUT_RING  (chan, fence->sequence);
		OUT_RING  (chan, NV84_SUBCHAN_SEMAPHORE_TRIGGER_ACQUIRE_GEQUAL);
		FIRE_RING (chan);
	}
	return ret;
}

static u32
nv84_fence_read(struct nouveau_channel *chan)
{
	struct drm_nouveau_private *dev_priv = chan->dev->dev_private;
	struct nv84_fence_priv *priv = dev_priv->fence.func;
	return nv_ro32(priv->mem, chan->id * 16);
}

static void
nv84_fence_context_del(struct nouveau_channel *chan)
{
	struct nv84_fence_chan *fctx = chan->fence;
	nouveau_fence_context_del(&fctx->base);
	chan->fence = NULL;
	kfree(fctx);
}

static int
nv84_fence_context_new(struct nouveau_channel *chan)
{
	struct drm_nouveau_private *dev_priv = chan->dev->dev_private;
	struct nv84_fence_priv *priv = dev_priv->fence.func;
	struct nv84_fence_chan *fctx;
	struct nouveau_gpuobj *obj;
	int ret, i;

	fctx = chan->fence = kzalloc(sizeof(*fctx), GFP_KERNEL);
	if (!fctx)
		return -ENOMEM;

	nouveau_fence_context_new(&fctx->base);

	ret = nouveau_gpuobj_dma_new(chan, NV_CLASS_DMA_FROM_MEMORY,
				     priv->mem->addr, priv->mem->size,
				     NV_MEM_ACCESS_RW,
				     NV_MEM_TARGET_VRAM, &obj);
	if (ret == 0) {
		ret = nouveau_ramht_insert(chan, NvSema, obj);
		nouveau_gpuobj_ref(NULL, &obj);
		nv_wo32(priv->mem, chan->id * 16, 0x00000000);
	}

	/* dma objects for display sync channel semaphore blocks */
	for (i = 0; i < chan->dev->mode_config.num_crtc; i++) {
		struct nv50_display *pdisp = nv50_display(chan->dev);
		struct nv50_display_crtc *dispc = &pdisp->crtc[i];
		struct nouveau_gpuobj *obj = NULL;

		ret = nouveau_gpuobj_dma_new(chan, NV_CLASS_DMA_IN_MEMORY,
					     dispc->sem.bo->bo.offset, 0x1000,
					     NV_MEM_ACCESS_RW,
					     NV_MEM_TARGET_VRAM, &obj);
		if (ret)
			break;

		ret = nouveau_ramht_insert(chan, NvEvoSema0 + i, obj);
		nouveau_gpuobj_ref(NULL, &obj);
	}

	if (ret)
		nv84_fence_context_del(chan);
	return ret;
}

static void
nv84_fence_destroy(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv84_fence_priv *priv = dev_priv->fence.func;

	nouveau_gpuobj_ref(NULL, &priv->mem);
	dev_priv->fence.func = NULL;
	kfree(priv);
}

int
nv84_fence_create(struct drm_device *dev)
{
	struct nouveau_fifo_priv *pfifo = nv_engine(dev, NVOBJ_ENGINE_FIFO);
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv84_fence_priv *priv;
	int ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base.dtor = nv84_fence_destroy;
	priv->base.context_new = nv84_fence_context_new;
	priv->base.context_del = nv84_fence_context_del;
	priv->base.emit = nv84_fence_emit;
	priv->base.sync = nv84_fence_sync;
	priv->base.read = nv84_fence_read;
	dev_priv->fence.func = priv;

	ret = nouveau_gpuobj_new(dev, NULL, 16 * pfifo->channels,
				 0x1000, 0, &priv->mem);
	if (ret)
		goto out;

out:
	if (ret)
		nv84_fence_destroy(dev);
	return ret;
}
