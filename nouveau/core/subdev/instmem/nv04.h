#ifndef __NV04_INSTMEM_H__
#define __NV04_INSTMEM_H__

#include <core/gpuobj.h>
#include <core/mm.h>

#include <subdev/instmem.h>

struct nv04_instmem_priv {
	struct nouveau_instmem base;
	bool created;

	void __iomem *iomem;
	struct nouveau_mm heap;

	struct nouveau_gpuobj *vbios;
	struct nouveau_gpuobj *ramht;
	struct nouveau_gpuobj *ramro;
	struct nouveau_gpuobj *ramfc;
};

struct nv04_instobj_priv {
	struct nouveau_instobj base;
	struct nouveau_mm_node *mem;
};

void nv04_instmem_dtor(struct nouveau_object *);

int nv04_instmem_alloc(struct nouveau_instmem *, struct nouveau_object *,
		       u32 size, u32 align, struct nouveau_object **pobject);

#endif
