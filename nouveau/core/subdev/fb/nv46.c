/*
 * Copyright (C) 2010 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <subdev/fb.h>

struct nv46_fb_priv {
	struct nouveau_fb base;
};

static int
nv46_fb_ctor(struct nouveau_object *parent, struct nouveau_object *engine,
	     struct nouveau_oclass *oclass, void *data, u32 size,
	     struct nouveau_object **pobject)
{
	struct nouveau_device *device = nv_device(parent);
	struct nv46_fb_priv *priv;
	u32 pfb474;
	int ret;

	ret = nouveau_fb_create(parent, engine, oclass, &priv);
	*pobject = nv_object(priv);
	if (ret)
		return ret;

	pfb474 = nv_rd32(priv, 0x100474);
	if (pfb474 & 0x00000004)
		priv->base.ram.type = NV_MEM_TYPE_GDDR3;
	if (pfb474 & 0x00000002)
		priv->base.ram.type = NV_MEM_TYPE_DDR2;
	if (pfb474 & 0x00000001)
		priv->base.ram.type = NV_MEM_TYPE_DDR1;

	priv->base.ram.size = nv_rd32(priv, 0x10020c) & 0xff000000;

	priv->base.memtype_valid = nv04_fb_memtype_valid;
	priv->base.tile.regions = 15;
	priv->base.tile.init = nv30_fb_tile_init;
	priv->base.tile.fini = nv30_fb_tile_fini;
	priv->base.tile.prog = nv41_fb_tile_prog;
	return nouveau_fb_created(&priv->base);
}


struct nouveau_oclass
nv46_fb_oclass = {
	.handle = NV_SUBDEV(FB, 0x46),
	.ofuncs = &(struct nouveau_ofuncs) {
		.ctor = nv46_fb_ctor,
		.dtor = _nouveau_fb_dtor,
		.init = nv44_fb_init,
		.fini = _nouveau_fb_fini,
	},
};
