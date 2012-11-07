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

#include <core/os.h>
#include <core/class.h>

#include <subdev/bios.h>
#include <subdev/bios/dcb.h>

#include "nv50.h"

int
nv50_sor_mthd(struct nouveau_object *object, u32 mthd, void *args, u32 size)
{
	struct nv50_disp_priv *priv = (void *)object->engine;
	struct nouveau_bios *bios = nouveau_bios(priv);
	const u16 type = (mthd & NV50_DISP_SOR_MTHD_TYPE) >> 12;
	const u8  head = (mthd & NV50_DISP_SOR_MTHD_HEAD) >> 3;
	const u8  link = (mthd & NV50_DISP_SOR_MTHD_LINK) >> 2;
	const u8    or = (mthd & NV50_DISP_SOR_MTHD_OR);
	const u16 mask = (0x0100 << head) | (0x0040 << link) | (0x0001 << or);
	struct dcb_output outp = {
		.type = type,
		.or = (1 << or),
		.sorconf.link = (1 << link),
	};
	u8  ver, hdr, idx = 0;
	u32 data;
	int ret = -EINVAL;

	if (size < sizeof(u32))
		return -EINVAL;

	while (type && (data = dcb_outp(bios, idx++, &ver, &hdr))) {
		u32 conn = nv_ro32(bios, data + 0);
		u32 conf = nv_ro32(bios, data + 4);
		if ((conn & 0x00300000) ||
		    (conn & 0x0000000f) != type ||
		    (conn & 0x0f000000) != (0x01000000 << or))
			continue;

		if ( (mask & 0x00c0) && (mask & 0x00c0) !=
		    ((mask & 0x00c0) & ((conf & 0x00000030) << 2)))
			continue;

		outp.connector = (conn & 0x0000f000) >> 12;
	}

	if (data == 0x0000)
		return -ENODEV;

	data = *(u32 *)args;
	switch (mthd & ~0x3f) {
	case NV94_DISP_SOR_DP_TRAIN:
		ret = priv->sor.dp_train(priv, or, link, type, mask, data, &outp);
		break;
	case NV94_DISP_SOR_DP_LNKCTL:
		ret = priv->sor.dp_lnkctl(priv, or, link, head, type, mask, data, &outp);
		break;
	case NV94_DISP_SOR_DP_DRVCTL(0):
	case NV94_DISP_SOR_DP_DRVCTL(1):
	case NV94_DISP_SOR_DP_DRVCTL(2):
	case NV94_DISP_SOR_DP_DRVCTL(3):
		ret = priv->sor.dp_drvctl(priv, or, link, (mthd & 0xc0) >> 6,
				          type, mask, data, &outp);
		break;
	default:
		BUG_ON(1);
	}

	return ret;
}
